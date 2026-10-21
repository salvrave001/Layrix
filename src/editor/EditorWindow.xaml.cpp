#include "pch.h"
#include "EditorWindow.xaml.h"
#include "DesktopZoneEditor.h"

#include "CanvasEditor.xaml.h"
#include "TemplateCard.xaml.h"
#include "engine/Messages.h"
#include "engine/StartupRegistration.h"

#include <microsoft.ui.xaml.window.h>

#include <algorithm>
#include <cmath>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Windowing;
using namespace winrt::Windows::UI;

namespace {
constexpr const wchar_t* kSwatches[] = {
    L"#4CC2FF", L"#3DDC97", L"#F0C14A", L"#FF7A59", L"#C792EA", L"#FF5C8A", L"#FFFFFF",
};

SolidColorBrush Paint(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return SolidColorBrush(ColorHelper::FromArgb(a, r, g, b));
}

bool ParseUiColor(std::wstring const& text, Color& color) {
    COLORREF value = 0;
    if (!layrix::ParseHexColor(text, value)) {
        return false;
    }
    color = ColorHelper::FromArgb(255, GetRValue(value), GetGValue(value), GetBValue(value));
    return true;
}
}

namespace winrt::LayrixEditor::implementation {

EditorWindow::~EditorWindow() = default;

EditorWindow::EditorWindow() : store_(layrix::LayoutStore::BesideExecutable()) {
    InitializeComponent();
    BuildRuntimeControls();
    ExtendsContentIntoTitleBar(true);
    SetTitleBar(TitleDrag());
    AppWindow().Resize({1100, 740});
    auto titleBar = AppWindow().TitleBar();
    titleBar.ButtonBackgroundColor(ColorHelper::FromArgb(0, 0, 0, 0));
    titleBar.ButtonForegroundColor(ColorHelper::FromArgb(255, 230, 230, 230));

    desktopEditor_ = std::make_unique<layrix::DesktopZoneEditor>();

    SettingsButton().Click([this](auto&&, auto&&) { ShowSettings(SettingsPage().Visibility() != Visibility::Visible); });
    BackToLayouts().Click([this](auto&&, auto&&) { ShowSettings(false); });
    StartupToggle().Toggled([this](auto&&, auto&&) {
        if (suppress_) return;
        const auto error = layrix::SetStartup(StartupToggle().IsOn());
        SyncStartup();
        if (error != ERROR_SUCCESS) {
            StartupStatus().Text(hstring{L"Не удалось изменить автозапуск. Код ошибки: " + std::to_wstring(error)});
        }
    });
    ZonesToggle().Toggled([this](auto&&, auto&&) {
        if (suppress_) return;
        const bool enabled = ZonesToggle().IsOn();
        Persist([enabled](layrix::AppSettings& settings) { settings.zonesEnabled = enabled; });
    });
    ArrowKeysToggle().Toggled([this](auto&&, auto&&) {
        if (suppress_) return;
        const bool enabled = ArrowKeysToggle().IsOn();
        Persist([enabled](layrix::AppSettings& settings) { settings.overrideWinArrows = enabled; });
    });

    spacingSlider_.ValueChanged([this](auto&&, auto&&) {
        if (suppress_) return;
        const int spacing = static_cast<int>(std::lround(spacingSlider_.Value()));
        SpacingValue().Text(hstring{std::to_wstring(spacing) + L" px"});
    });
    spacingSlider_.PointerReleased([this](auto&&, auto&&) { CommitSpacing(); });
    spacingSlider_.LostFocus([this](auto&&, auto&&) { CommitSpacing(); });
    winArrowSwitch_.Toggled([this](auto&&, auto&&) {
        if (suppress_) return;
        const bool enabled = winArrowSwitch_.IsOn();
        Persist([enabled](layrix::AppSettings& settings) { settings.overrideWinArrows = enabled; });
    });
    colorBox_.LostFocus([this](auto&&, auto&&) { CommitColor(); });
    colorBox_.KeyDown([this](auto&&, KeyRoutedEventArgs const& args) {
        if (args.Key() == Windows::System::VirtualKey::Enter) {
            CommitColor();
            args.Handled(true);
        }
    });
    CreateButton().Click([this](auto&&, auto&&) {
        ShowCanvas(layrix::NextCustomName(settings_), L"", {}, true);
    });
    Root().KeyDown([this](auto&&, KeyRoutedEventArgs const& args) {
        if (args.Key() == Windows::System::VirtualKey::Escape && editing_) {
            HideCanvas();
            args.Handled(true);
        }
    });

    try {
        auto native = this->try_as<IWindowNative>();
        HWND hwnd = nullptr;
        if (native && SUCCEEDED(native->get_WindowHandle(&hwnd)) && hwnd) {
            if (HICON icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1))) {
                SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
                SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
            }
        }
    } catch (...) {
    }

    ReloadFromDisk();
    FlashSelected();
}

void EditorWindow::BuildRuntimeControls() {
    spacingSlider_ = Slider();
    spacingSlider_.Width(140);
    spacingSlider_.Minimum(0);
    spacingSlider_.Maximum(32);
    spacingSlider_.StepFrequency(1);
    spacingSlider_.TickFrequency(1);
    spacingSlider_.VerticalAlignment(VerticalAlignment::Center);

    colorBox_ = TextBox();
    colorBox_.Width(92);
    colorBox_.PlaceholderText(L"#4CC2FF");

    winArrowSwitch_ = ToggleSwitch();
    winArrowSwitch_.MinWidth(0);
    winArrowSwitch_.OnContent(box_value(hstring{}));
    winArrowSwitch_.OffContent(box_value(hstring{}));

    auto children = ControlsHost().Children();
    // Spacing group: label, slider, value
    if (auto spacingGroup = children.GetAt(0).try_as<StackPanel>()) {
        spacingGroup.Children().InsertAt(1, spacingSlider_);
    }
    // Color group: label, host, textbox
    if (auto colorGroup = children.GetAt(1).try_as<StackPanel>()) {
        colorGroup.Children().Append(colorBox_);
    }
    // Hotkey group: label, switch
    if (auto hotkeyGroup = children.GetAt(2).try_as<StackPanel>()) {
        hotkeyGroup.Children().Append(winArrowSwitch_);
    }
}

void EditorWindow::ReloadFromDisk() {
    settings_ = store_.Load();
    monitors_ = layrix::EnumerateMonitors();
    if (selected_ >= static_cast<int>(monitors_.size())) {
        selected_ = 0;
    }
    if (!editing_) {
        RebuildMonitorChips();
        RebuildCards();
        SyncControls();
    }
}

void EditorWindow::Persist(std::function<void(layrix::AppSettings&)> change) {
    store_.Update(std::move(change));
    NotifyEngine();
    ReloadFromDisk();
}

void EditorWindow::NotifyEngine() const {
    if (HWND engine = FindWindowW(layrix::kEngineClass, L"Layrix")) {
        PostMessageW(engine, layrix::kMsgReload, 0, 0);
    }
}

void EditorWindow::FlashSelected() const {
    if (HWND engine = FindWindowW(layrix::kEngineClass, L"Layrix")) {
        PostMessageW(engine, layrix::kMsgFlash, static_cast<WPARAM>(selected_), 0);
    }
}

std::wstring EditorWindow::AssignedId() const {
    if (selected_ < 0 || selected_ >= static_cast<int>(monitors_.size())) {
        return L"columns";
    }
    const auto& monitor = monitors_[static_cast<size_t>(selected_)];
    return settings_.LayoutForMonitor(monitor.key, monitor.device);
}

void EditorWindow::RebuildMonitorChips() {
    MonitorHost().Children().Clear();
    for (int index = 0; index < static_cast<int>(monitors_.size()); ++index) {
        Button chip;
        chip.Content(box_value(hstring{std::to_wstring(index + 1)}));
        const auto& bounds = monitors_[static_cast<size_t>(index)].bounds;
        const double aspect = static_cast<double>(bounds.right - bounds.left) /
            std::max(1L, bounds.bottom - bounds.top);
        chip.Width(std::clamp(48.0 * aspect, 32.0, 100.0));
        chip.Height(48);
        chip.FontSize(16);
        chip.CornerRadius({3, 3, 3, 3});
        const bool active = index == selected_;
        chip.Background(active ? Paint(30, 48, 64) : Paint(42, 42, 42));
        chip.Foreground(active ? Paint(76, 194, 255) : Paint(243, 243, 243));
        chip.BorderBrush(active ? Paint(76, 194, 255) : Paint(58, 58, 58));
        const double border = active ? 2.0 : 1.0;
        chip.BorderThickness(Thickness{border, border, border, border});
        chip.Click([this, index](auto&&, auto&&) { SelectMonitor(index); });
        MonitorHost().Children().Append(chip);
    }
    if (!monitors_.empty()) {
        const auto& bounds = monitors_[static_cast<size_t>(selected_)].bounds;
        MonitorDescription().Text(hstring{L"Монитор " + std::to_wstring(selected_ + 1) +
            L" · " + std::to_wstring(bounds.right - bounds.left) + L" × " +
            std::to_wstring(bounds.bottom - bounds.top)});
    } else {
        MonitorDescription().Text(L"Мониторы не найдены");
    }
}

void EditorWindow::SelectMonitor(int index) {
    if (index < 0 || index >= static_cast<int>(monitors_.size())) {
        return;
    }
    selected_ = index;
    RebuildMonitorChips();
    RebuildCards();
    SyncControls();
    FlashSelected();
}

void EditorWindow::RebuildCards() {
    TemplateHost().Children().Clear();
    CustomHost().Children().Clear();
    const std::wstring assigned = AssignedId();
    int customCount = 0;
    for (const auto& layout : settings_.layouts) {
        auto card = make<TemplateCard>();
        auto* view = get_self<TemplateCard>(card);
        view->SetTitle(hstring{layout.name});
        view->SetZones(layrix::BuildNormalizedZones(layout));
        view->SetSelected(layout.id == assigned);
        const std::wstring id = layout.id;
        view->SetOnClick([this, id] { AssignLayout(id); });
        view->SetOnEdit([this, id] { EditLayout(id); });
        if (layout.builtin) {
            TemplateHost().Children().Append(card);
        } else {
            CustomHost().Children().Append(card);
            ++customCount;
        }
    }
    CustomEmpty().Visibility(customCount == 0 ? Visibility::Visible : Visibility::Collapsed);
}

void EditorWindow::SyncControls() {
    suppress_ = true;
    const layrix::Layout* layout = settings_.FindLayout(AssignedId());
    const int spacing = layout ? std::clamp(layout->spacing, 0, 32) : 8;
    spacingSlider_.Value(spacing);
    SpacingValue().Text(hstring{std::to_wstring(spacing) + L" px"});
    SpacingLabel().Text(L"Отступ между зонами");
    winArrowSwitch_.IsOn(settings_.overrideWinArrows);
    ZonesToggle().IsOn(settings_.zonesEnabled);
    ArrowKeysToggle().IsOn(settings_.overrideWinArrows);
    colorBox_.Text(settings_.highlightColor);
    ColorHost().Children().Clear();
    for (const wchar_t* swatch : kSwatches) {
        const std::wstring hex = swatch;
        Color color{};
        if (!ParseUiColor(hex, color)) {
            continue;
        }
        Button button;
        button.Width(22);
        button.Height(22);
        button.Padding(Thickness{0});
        button.CornerRadius({11, 11, 11, 11});
        button.Background(SolidColorBrush(color));
        const bool selected = _wcsicmp(hex.c_str(), settings_.highlightColor.c_str()) == 0;
        button.BorderBrush(selected ? Paint(255, 255, 255) : Paint(0, 0, 0, 0));
        const double border = selected ? 2.0 : 0.0;
        button.BorderThickness(Thickness{border, border, border, border});
        button.Click([this, hex](auto&&, auto&&) {
            Persist([hex](layrix::AppSettings& settings) { settings.highlightColor = hex; });
        });
        ColorHost().Children().Append(button);
    }
    suppress_ = false;
    SyncStartup();
}

void EditorWindow::SyncStartup() {
    const auto state = layrix::ReadStartup();
    const bool previous = suppress_;
    suppress_ = true;
    StartupToggle().IsOn(state.enabled);
    suppress_ = previous;
    // Leave the switch usable so a broken registration can be repaired.
    StartupToggle().IsEnabled(true);
    StartupStatus().Text(state.error != ERROR_SUCCESS ?
        hstring{L"Не удалось прочитать настройку автозапуска. Код ошибки: " + std::to_wstring(state.error)} :
        hstring{state.legacy ? L"Старая запись автозапуска: выключите и снова включите переключатель для восстановления." :
            (state.enabled ? L"Автозапуск настроен через папку «Автозагрузка» текущего пользователя." : L"Автозапуск выключен.")});
}

bool EditorWindow::ShowSettings(bool show) {
    if (editing_) return false;
    ReloadFromDisk();
    SettingsPage().Visibility(show ? Visibility::Visible : Visibility::Collapsed);
    Gallery().Visibility(show ? Visibility::Collapsed : Visibility::Visible);
    MonitorSection().Visibility(show ? Visibility::Collapsed : Visibility::Visible);
    BottomBar().Visibility(show ? Visibility::Collapsed : Visibility::Visible);
    Title(show ? L"Layrix — Настройки" : L"Layrix — Редактор зон");
    return true;
}

void EditorWindow::AssignLayout(std::wstring const& id) {
    if (selected_ < 0 || selected_ >= static_cast<int>(monitors_.size())) {
        return;
    }
    const std::wstring key = monitors_[static_cast<size_t>(selected_)].key;
    Persist([&](layrix::AppSettings& settings) { settings.SetMonitorLayout(key, id); });
}

void EditorWindow::EditLayout(std::wstring const& id) {
    const auto* layout = settings_.FindLayout(id);
    if (!layout) return;
    // Built-in templates remain available; screen edits create a custom copy.
    ShowCanvas(layout->builtin ? layrix::NextCustomName(settings_) : layout->name,
        layout->builtin ? L"" : layout->id, layrix::BuildNormalizedZones(*layout), layout->builtin);
}
void EditorWindow::ShowCanvas(std::wstring const& name, std::wstring const& id, std::vector<layrix::NormRect> const& zones, bool isNew) {
    if (editing_ || selected_ < 0 || selected_ >= static_cast<int>(monitors_.size())) return;
    editing_ = true;
    draftNew_ = isNew;
    draftId_ = id;
    const auto* layout = settings_.FindLayout(id.empty() ? AssignedId() : id);
    draftSpacing_ = layout ? layout->spacing : 8;
    COLORREF color = RGB(76, 194, 255);
    layrix::ParseHexColor(settings_.highlightColor, color);
    HWND owner = nullptr;
    this->try_as<IWindowNative>()->get_WindowHandle(&owner);
    auto weak = get_weak();
    auto queue = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
    AppWindow().Hide();
    if (!desktopEditor_->Open(owner, monitors_[static_cast<size_t>(selected_)].work, name, zones,
        draftSpacing_, color, [weak, queue](bool save) {
            queue.TryEnqueue([weak, save] {
                if (auto window = weak.get()) {
                    if (save) window->SaveCanvas(); else window->HideCanvas();
                }
            });
        })) {
        desktopEditor_->Close();
        HideCanvas();
        MessageBoxW(owner, L"Не удалось открыть редактирование зон на рабочем столе.", L"Layrix", MB_ICONERROR);
    }
}

void EditorWindow::HideCanvas() {
    desktopEditor_->Close();
    editing_ = false;
    draftNew_ = false;
    draftId_.clear();
    ReloadFromDisk();
    AppWindow().Show();
    Activate();
}

void EditorWindow::SaveCanvas() {
    std::wstring name = desktopEditor_->Name();
    if (name.empty()) {
        name = draftNew_ ? layrix::NextCustomName(settings_) : L"Свой макет";
    }
    const auto zones = desktopEditor_->Zones();
    if (zones.empty()) { HideCanvas(); return; }
    const std::wstring id = draftId_;
    const bool isNew = draftNew_;
    std::wstring key;
    if (selected_ >= 0 && selected_ < static_cast<int>(monitors_.size())) {
        key = monitors_[static_cast<size_t>(selected_)].key;
    }
    editing_ = false;
    Persist([&](layrix::AppSettings& settings) {
        if (isNew) {
            layrix::Layout layout;
            layout.id = layrix::NextCustomId(settings);
            layout.name = name;
            layout.type = layrix::LayoutType::Canvas;
            layout.spacing = draftSpacing_;
            layout.zones = zones;
            layout.builtin = false;
            if (!key.empty()) {
                settings.SetMonitorLayout(key, layout.id);
            }
            settings.layouts.push_back(std::move(layout));
        } else if (auto* layout = settings.FindLayout(id)) {
            layout->name = name;
            layout->zones = zones;
            layout->type = layrix::LayoutType::Canvas;
            if (!key.empty()) settings.SetMonitorLayout(key, id);
        }
    });
    HideCanvas();
}

void EditorWindow::DeleteCanvas() {
    if (draftNew_) {
        HideCanvas();
        return;
    }
    const std::wstring id = draftId_;
    editing_ = false;
    Persist([&](layrix::AppSettings& settings) {
        std::erase_if(settings.layouts, [&](const layrix::Layout& layout) { return layout.id == id; });
        for (auto& entry : settings.monitorLayouts) {
            if (entry.second == id) {
                entry.second = L"columns";
            }
        }
    });
    HideCanvas();
}

void EditorWindow::CommitSpacing() {
    if (suppress_) {
        return;
    }
    const int spacing = std::clamp(static_cast<int>(std::lround(spacingSlider_.Value())), 0, 32);
    const std::wstring id = AssignedId();
    const layrix::Layout* layout = settings_.FindLayout(id);
    if (layout && layout->spacing == spacing) {
        return;
    }
    Persist([id, spacing](layrix::AppSettings& settings) {
        if (auto* item = settings.FindLayout(id)) {
            item->spacing = spacing;
        }
    });
}

void EditorWindow::CommitColor() {
    if (suppress_) {
        return;
    }
    std::wstring text{colorBox_.Text()};
    COLORREF color = 0;
    if (!layrix::ParseHexColor(text, color)) {
        suppress_ = true;
        colorBox_.Text(settings_.highlightColor);
        suppress_ = false;
        return;
    }
    text = layrix::ColorToHex(color);
    if (_wcsicmp(text.c_str(), settings_.highlightColor.c_str()) == 0) {
        return;
    }
    Persist([text](layrix::AppSettings& settings) { settings.highlightColor = text; });
}

}  // namespace winrt::LayrixEditor::implementation
