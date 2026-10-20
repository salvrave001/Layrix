#include "pch.h"
#include "App.xaml.h"

#include "EditorApi.h"
#include "EditorWindow.xaml.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Windowing;

namespace {
struct EditorHost {
    std::mutex mutex;
    std::condition_variable ready;
    Microsoft::UI::Dispatching::DispatcherQueue queue{nullptr};
    bool dispatcherReady = false;
    bool threadStarted = false;
    Window stub{nullptr};
    LayrixEditor::EditorWindow editor{nullptr};
};

EditorHost& Host() {
    static EditorHost host;
    return host;
}

void LoadFrameworkThemes(Application const& app) {
    auto merged = app.Resources().MergedDictionaries();
    for (wchar_t const* uri : {
             L"ms-appx:///Microsoft.UI.Xaml/Themes/themeresources.xaml",
             L"ms-appx:///Microsoft.UI.Xaml/Themes/generic.xaml",
         }) {
        try {
            ResourceDictionary dictionary;
            dictionary.Source(Windows::Foundation::Uri{uri});
            merged.Append(dictionary);
        } catch (...) {
        }
    }
}

void OpenEditorWindow(bool settings) {
    auto& host = Host();
    if (host.editor) {
        if (winrt::get_self<winrt::LayrixEditor::implementation::EditorWindow>(host.editor)->ShowSettings(settings)) {
            host.editor.Activate();
        }
        return;
    }
    try {
        host.editor = make<LayrixEditor::implementation::EditorWindow>();
    } catch (hresult_error const& error) {
        std::wstring text = L"Не удалось открыть редактор зон.\n";
        text += error.message().c_str();
        wchar_t code[32]{};
        swprintf_s(code, L"\nHRESULT: 0x%08X", static_cast<unsigned int>(error.code().value));
        text += code;
        MessageBoxW(nullptr, text.c_str(), L"Layrix", MB_ICONERROR);
        return;
    } catch (...) {
        MessageBoxW(nullptr, L"Не удалось открыть редактор зон.", L"Layrix", MB_ICONERROR);
        return;
    }
    host.editor.Closed([](auto&&, auto&&) {
        if (auto queue = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread()) {
            queue.TryEnqueue([] { Host().editor = nullptr; });
        } else {
            Host().editor = nullptr;
        }
    });
    winrt::get_self<winrt::LayrixEditor::implementation::EditorWindow>(host.editor)->ShowSettings(settings);
    host.editor.Activate();
}

void EditorThread() {
    try {
        init_apartment(apartment_type::single_threaded);
        Application::Start([](auto&&) { make<LayrixEditor::implementation::App>(); });
    } catch (...) {
        OutputDebugStringW(L"Layrix editor failed to start\n");
    }
    std::lock_guard lock(Host().mutex);
    Host().dispatcherReady = false;
    Host().threadStarted = false;
    Host().queue = nullptr;
    Host().ready.notify_all();
}
}

namespace winrt::LayrixEditor::implementation {

App::App() {
    UnhandledException([](auto&&, UnhandledExceptionEventArgs const& args) {
        OutputDebugStringW(args.Message().c_str());
        args.Handled(true);
    });
    InitializeComponent();
}

void App::OnLaunched(LaunchActivatedEventArgs const&) {
    auto& host = Host();
    LoadFrameworkThemes(*this);
    try {
        host.stub = Window();
        host.stub.Content(Grid());
        host.stub.AppWindow().IsShownInSwitchers(false);
        if (auto presenter = host.stub.AppWindow().Presenter().try_as<OverlappedPresenter>()) {
            presenter.SetBorderAndTitleBar(false, false);
        }
        host.stub.AppWindow().Resize({1, 1});
        host.stub.AppWindow().Move({-32000, -32000});
        host.stub.Activate();
        host.stub.AppWindow().Hide();
    } catch (...) {
        OutputDebugStringW(L"Layrix editor host window failed\n");
    }

    auto queue = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
    {
        std::lock_guard lock(host.mutex);
        host.queue = queue;
        host.dispatcherReady = true;
    }
    host.ready.notify_all();
}

}  // namespace winrt::LayrixEditor::implementation

void RequestShowEditor(bool settings) {
    std::thread([settings] {
        auto& host = Host();
        std::unique_lock lock(host.mutex);
        if (!host.threadStarted) {
            host.threadStarted = true;
            std::thread(EditorThread).detach();
        }
        if (!host.ready.wait_for(lock, std::chrono::seconds(20), [&host] { return host.dispatcherReady; })) {
            MessageBoxW(nullptr, L"Не удалось открыть редактор зон.", L"Layrix", MB_ICONERROR);
            return;
        }
        auto queue = host.queue;
        lock.unlock();
        if (!queue || !queue.TryEnqueue([settings] { OpenEditorWindow(settings); })) {
            MessageBoxW(nullptr, L"Не удалось открыть редактор зон.", L"Layrix", MB_ICONERROR);
        }
    }).detach();
}
