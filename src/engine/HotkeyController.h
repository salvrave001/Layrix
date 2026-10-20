#pragma once

#include <windows.h>

namespace layrix {

class HotkeyController {
public:
    explicit HotkeyController(HWND host);
    ~HotkeyController();

    void SetEnabled(bool enabled);
    void SetTrackingCancel(bool tracking);
    bool ConsumeCancel();

private:
    static LRESULT CALLBACK Proc(int code, WPARAM wParam, LPARAM lParam);
    bool OnKey(const KBDLLHOOKSTRUCT& info, bool keyUp);

    HWND host_ = nullptr;
    HHOOK hook_ = nullptr;
    bool enabled_ = false;
    bool trackCancel_ = false;
    bool cancel_ = false;
    bool leftHeld_ = false;
    bool rightHeld_ = false;
};

}  // namespace layrix
