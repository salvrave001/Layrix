#include "app/TrayApp.h"

#include <shellapi.h>

#include "app/resource.h"
#include "engine/Messages.h"
#include "engine/SnapEngine.h"

#include <filesystem>

namespace layrix {

TrayApp::TrayApp() = default;

TrayApp::~TrayApp() {
    RemoveTray();
}

int TrayApp::Run(bool openEditor, bool openSettings) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = kEngineClass;
    windowClass.hIcon = LoadIconW(windowClass.hInstance, MAKEINTRESOURCEW(IDI_APP));
    RegisterClassW(&windowClass);

    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kEngineClass,
        L"Layrix",
        WS_POPUP,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        windowClass.hInstance,
        this);
    engine_ = std::make_unique<SnapEngine>(hwnd_);
    AddTray();
    if (openEditor || openSettings) {
        PostMessageW(hwnd_, kMsgOpenEditor, openSettings ? 1 : 0, 0);
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    engine_.reset();
    CoUninitialize();
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK TrayApp::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* created = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created->lpCreateParams));
    }
    auto* app = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!app) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return app->Handle(hwnd, message, wParam, lParam);
}

LRESULT TrayApp::Handle(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case kMsgReload:
        if (engine_) engine_->Reload();
        return 0;
    case kMsgOpenEditor:
        OpenEditor(wParam != 0);
        return 0;
    case kMsgFlash:
        if (engine_) engine_->FlashMonitor(static_cast<int>(wParam));
        return 0;
    case kMsgHover:
        if (engine_) engine_->OnHover();
        return 0;
    case kMsgDragEnd:
        if (engine_) engine_->EndDrag();
        return 0;
    case kMsgHotkey:
        if (engine_) engine_->MoveForegroundBy(static_cast<int>(wParam));
        return 0;
    case kMsgTray:
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK || LOWORD(lParam) == WM_LBUTTONUP) {
            if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
                OpenEditor();
            }
        } else if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            ShowMenu();
        }
        return 0;
    case WM_TIMER:
        if (wParam == kTimerSnap && engine_) engine_->FinishPendingSnap();
        if (wParam == kTimerBadge && engine_) engine_->HideBadge();
        if (wParam == kTimerDrag && engine_) engine_->OnDragTick();
        return 0;
    case WM_DESTROY:
        RemoveTray();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void TrayApp::AddTray() {
    const int width = GetSystemMetrics(SM_CXSMICON);
    const int height = GetSystemMetrics(SM_CYSMICON);
    icon_ = {};
    icon_.cbSize = sizeof(icon_);
    icon_.hWnd = hwnd_;
    icon_.uID = 1;
    icon_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    icon_.uCallbackMessage = kMsgTray;
    icon_.hIcon = static_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDI_APP),
        IMAGE_ICON,
        width,
        height,
        LR_DEFAULTCOLOR));
    if (!icon_.hIcon) {
        icon_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    wcscpy_s(icon_.szTip, L"Layrix");
    Shell_NotifyIconW(NIM_ADD, &icon_);
}

void TrayApp::RemoveTray() {
    if (icon_.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &icon_);
        icon_.hWnd = nullptr;
    }
    if (icon_.hIcon) {
        DestroyIcon(icon_.hIcon);
        icon_.hIcon = nullptr;
    }
}

void TrayApp::ShowMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"Редактор зон");
    AppendMenuW(menu, MF_STRING, 4, L"Настройки");
    const UINT check = (engine_ && engine_->ZonesEnabled()) ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(menu, MF_STRING | check, 2, L"Зоны включены");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, L"Выход");

    POINT point{};
    GetCursorPos(&point);
    SetForegroundWindow(hwnd_);
    const int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
    if (command == 1) {
        OpenEditor();
    } else if (command == 4) {
        OpenEditor(true);
    } else if (command == 2 && engine_) {
        engine_->SetZonesEnabled(!engine_->ZonesEnabled());
    } else if (command == 3) {
        DestroyWindow(hwnd_);
    }
}

void TrayApp::OpenEditor(bool settings) {
    if (!showEditor_) {
        wchar_t executable[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, executable, MAX_PATH);
        const auto dll = std::filesystem::path(executable).parent_path() / L"LayrixEditor.dll";
        editorModule_ = LoadLibraryW(dll.c_str());
        if (!editorModule_) {
            MessageBoxW(hwnd_, L"Не удалось открыть редактор зон.", L"Layrix", MB_ICONERROR);
            return;
        }
        showEditor_ = reinterpret_cast<void(__stdcall*)()>(GetProcAddress(editorModule_, "ShowLayrixEditor"));
        showSettings_ = reinterpret_cast<void(__stdcall*)()>(GetProcAddress(editorModule_, "ShowLayrixSettings"));
        if (!showEditor_) {
            MessageBoxW(hwnd_, L"Не удалось открыть редактор зон.", L"Layrix", MB_ICONERROR);
            return;
        }
    }
    if (settings && showSettings_) showSettings_(); else showEditor_();
}

}  // namespace layrix
