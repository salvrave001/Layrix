#pragma once

#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <optional>
#include <vector>

namespace layrix {

class ZoneOverlay {
public:
    ZoneOverlay();
    ~ZoneOverlay();

    void Show(const RECT& work, const std::vector<RECT>& zones, const std::optional<RECT>& highlight, COLORREF accent, bool topmost = true);
    void Hide();

private:
    bool EnsureWindow();
    bool EnsureSurface(int width, int height);
    void ReleaseSurface();
    static LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
    HDC screenDc_ = nullptr;
    HDC memoryDc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HBITMAP previousBitmap_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool visible_ = false;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> target_;
};

class MonitorBadge {
public:
    MonitorBadge();
    ~MonitorBadge();

    void Show(const RECT& monitor, int number, COLORREF accent);
    void Hide();

private:
    bool EnsureWindow();
    bool EnsureSurface(int size);
    void ReleaseSurface();
    static LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
    HDC screenDc_ = nullptr;
    HDC memoryDc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HBITMAP previousBitmap_ = nullptr;
    int size_ = 0;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> write_;
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> target_;
};

}  // namespace layrix
