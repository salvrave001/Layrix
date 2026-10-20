#include "engine/HotkeyController.h"

#include "engine/Messages.h"

namespace layrix {
namespace {
HotkeyController* g_hotkeys = nullptr;
}

HotkeyController::HotkeyController(HWND host) : host_(host) {
    g_hotkeys = this;
    hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, Proc, GetModuleHandleW(nullptr), 0);
}

HotkeyController::~HotkeyController() {
    if (hook_) {
        UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
    }
    if (g_hotkeys == this) {
        g_hotkeys = nullptr;
    }
}

void HotkeyController::SetEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) {
        leftHeld_ = false;
        rightHeld_ = false;
    }
}

void HotkeyController::SetTrackingCancel(bool tracking) {
    trackCancel_ = tracking;
    if (tracking) {
        cancel_ = false;
    }
}

bool HotkeyController::ConsumeCancel() {
    const bool value = cancel_;
    cancel_ = false;
    return value;
}

LRESULT CALLBACK HotkeyController::Proc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_hotkeys) {
        const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        const bool keyUp = (info->flags & LLKHF_UP) != 0;
        if (g_hotkeys->OnKey(*info, keyUp)) {
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

bool HotkeyController::OnKey(const KBDLLHOOKSTRUCT& info, bool keyUp) {
    if (info.vkCode == VK_ESCAPE && !keyUp && trackCancel_) {
        cancel_ = true;
    }
    if (info.vkCode == VK_LEFT && keyUp) {
        leftHeld_ = false;
    }
    if (info.vkCode == VK_RIGHT && keyUp) {
        rightHeld_ = false;
    }
    if (keyUp || !enabled_) {
        return false;
    }
    if (info.vkCode != VK_LEFT && info.vkCode != VK_RIGHT) {
        return false;
    }

    const bool win = (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);
    if (!win) {
        return false;
    }
    if ((GetAsyncKeyState(VK_CONTROL) | GetAsyncKeyState(VK_MENU) | GetAsyncKeyState(VK_SHIFT)) & 0x8000) {
        return false;
    }

    bool& held = info.vkCode == VK_LEFT ? leftHeld_ : rightHeld_;
    if (!held) {
        held = true;
        const WPARAM direction = info.vkCode == VK_LEFT ? static_cast<WPARAM>(-1) : static_cast<WPARAM>(1);
        PostMessageW(host_, kMsgHotkey, direction, 0);
    }
    return true;
}

}  // namespace layrix
