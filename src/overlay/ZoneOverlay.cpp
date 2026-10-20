#include "overlay/ZoneOverlay.h"

#include "engine/Messages.h"

#include <algorithm>
#include <string>

namespace layrix {
namespace {

void RegisterOnce(const wchar_t* className, WNDPROC proc) {
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = proc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = className;
    RegisterClassW(&windowClass);
}

HWND CreateLayered(const wchar_t* className, int x, int y, int width, int height) {
    return CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        className,
        L"",
        WS_POPUP,
        x,
        y,
        width,
        height,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
}

bool CreateBitmapTarget(
    ID2D1Factory* factory,
    HDC screenDc,
    HDC& memoryDc,
    HBITMAP& bitmap,
    HBITMAP& previous,
    int width,
    int height,
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget>& target) {
    memoryDc = CreateCompatibleDC(screenDc);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    bitmap = CreateDIBSection(screenDc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!memoryDc || !bitmap) {
        return false;
    }
    previous = static_cast<HBITMAP>(SelectObject(memoryDc, bitmap));

    const auto properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f);
    if (FAILED(factory->CreateDCRenderTarget(&properties, target.GetAddressOf()))) {
        return false;
    }
    RECT bounds{0, 0, width, height};
    return SUCCEEDED(target->BindDC(memoryDc, &bounds));
}

void Present(HWND hwnd, HDC screenDc, HDC memoryDc, int x, int y, int width, int height) {
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    POINT destination{x, y};
    SIZE size{width, height};
    POINT source{0, 0};
    UpdateLayeredWindow(hwnd, screenDc, &destination, &size, memoryDc, &source, 0, &blend, ULW_ALPHA);
}

D2D1::ColorF FromAccent(COLORREF color, float alpha) {
    return D2D1::ColorF(
        GetRValue(color) / 255.0f,
        GetGValue(color) / 255.0f,
        GetBValue(color) / 255.0f,
        alpha);
}

D2D1_ROUNDED_RECT Round(float left, float top, float right, float bottom, float radius) {
    D2D1_ROUNDED_RECT rect{};
    rect.rect = D2D1::RectF(left, top, right, bottom);
    rect.radiusX = radius;
    rect.radiusY = radius;
    return rect;
}

}  // namespace

ZoneOverlay::ZoneOverlay() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf());
}

ZoneOverlay::~ZoneOverlay() {
    Hide();
    ReleaseSurface();
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

void ZoneOverlay::ReleaseSurface() {
    target_.Reset();
    if (memoryDc_ && previousBitmap_) {
        SelectObject(memoryDc_, previousBitmap_);
    }
    if (bitmap_) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    if (memoryDc_) {
        DeleteDC(memoryDc_);
        memoryDc_ = nullptr;
    }
    if (screenDc_) {
        ReleaseDC(nullptr, screenDc_);
        screenDc_ = nullptr;
    }
    previousBitmap_ = nullptr;
    width_ = 0;
    height_ = 0;
}

bool ZoneOverlay::EnsureWindow() {
    if (hwnd_) {
        return true;
    }
    RegisterOnce(kOverlayClass, Proc);
    hwnd_ = CreateLayered(kOverlayClass, 0, 0, 0, 0);
    return hwnd_ != nullptr;
}

bool ZoneOverlay::EnsureSurface(int width, int height) {
    if (!factory_ || width <= 0 || height <= 0) {
        return false;
    }
    if (target_ && width_ == width && height_ == height) {
        RECT bounds{0, 0, width, height};
        return SUCCEEDED(target_->BindDC(memoryDc_, &bounds));
    }
    ReleaseSurface();
    screenDc_ = GetDC(nullptr);
    if (!CreateBitmapTarget(factory_.Get(), screenDc_, memoryDc_, bitmap_, previousBitmap_, width, height, target_)) {
        ReleaseSurface();
        return false;
    }
    width_ = width;
    height_ = height;
    return true;
}

void ZoneOverlay::Show(const RECT& work, const std::vector<RECT>& zones, const std::optional<RECT>& highlight, COLORREF accent) {
    const int width = work.right - work.left;
    const int height = work.bottom - work.top;
    if (!EnsureWindow() || !EnsureSurface(width, height)) {
        return;
    }

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> idleFill;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> idleLine;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> hotFill;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> hotLine;
    target_->CreateSolidColorBrush(D2D1::ColorF(0.72f, 0.84f, 0.94f, 0.16f), idleFill.GetAddressOf());
    target_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.38f), idleLine.GetAddressOf());
    target_->CreateSolidColorBrush(FromAccent(accent, 0.38f), hotFill.GetAddressOf());
    target_->CreateSolidColorBrush(FromAccent(accent, 0.95f), hotLine.GetAddressOf());

    target_->BeginDraw();
    target_->Clear(D2D1::ColorF(0, 0, 0, 0));
    for (const RECT& zone : zones) {
        const auto shape = Round(
            static_cast<float>(zone.left - work.left) + 1.0f,
            static_cast<float>(zone.top - work.top) + 1.0f,
            static_cast<float>(zone.right - work.left) - 1.0f,
            static_cast<float>(zone.bottom - work.top) - 1.0f,
            10.0f);
        target_->FillRoundedRectangle(shape, idleFill.Get());
        target_->DrawRoundedRectangle(shape, idleLine.Get(), 1.5f);
    }
    if (highlight) {
        const RECT& zone = *highlight;
        const auto shape = Round(
            static_cast<float>(zone.left - work.left) + 1.0f,
            static_cast<float>(zone.top - work.top) + 1.0f,
            static_cast<float>(zone.right - work.left) - 1.0f,
            static_cast<float>(zone.bottom - work.top) - 1.0f,
            12.0f);
        target_->FillRoundedRectangle(shape, hotFill.Get());
        target_->DrawRoundedRectangle(shape, hotLine.Get(), 2.5f);
    }
    target_->EndDraw();
    Present(hwnd_, screenDc_, memoryDc_, work.left, work.top, width, height);
    if (!visible_) {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        visible_ = true;
    }
}

void ZoneOverlay::Hide() {
    if (hwnd_ && visible_) {
        ShowWindow(hwnd_, SW_HIDE);
        visible_ = false;
    }
}

LRESULT CALLBACK ZoneOverlay::Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCHITTEST) {
        return HTTRANSPARENT;
    }
    if (message == WM_MOUSEACTIVATE) {
        return MA_NOACTIVATE;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

MonitorBadge::MonitorBadge() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf());
    DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(write_.GetAddressOf()));
}

MonitorBadge::~MonitorBadge() {
    Hide();
    ReleaseSurface();
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

void MonitorBadge::ReleaseSurface() {
    target_.Reset();
    if (memoryDc_ && previousBitmap_) {
        SelectObject(memoryDc_, previousBitmap_);
    }
    if (bitmap_) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    if (memoryDc_) {
        DeleteDC(memoryDc_);
        memoryDc_ = nullptr;
    }
    if (screenDc_) {
        ReleaseDC(nullptr, screenDc_);
        screenDc_ = nullptr;
    }
    previousBitmap_ = nullptr;
    size_ = 0;
}

bool MonitorBadge::EnsureWindow() {
    if (hwnd_) {
        return true;
    }
    RegisterOnce(kBadgeClass, Proc);
    hwnd_ = CreateLayered(kBadgeClass, 0, 0, 0, 0);
    return hwnd_ != nullptr;
}

bool MonitorBadge::EnsureSurface(int size) {
    if (!factory_ || size <= 0) {
        return false;
    }
    if (target_ && size_ == size) {
        RECT bounds{0, 0, size, size};
        return SUCCEEDED(target_->BindDC(memoryDc_, &bounds));
    }
    ReleaseSurface();
    screenDc_ = GetDC(nullptr);
    if (!CreateBitmapTarget(factory_.Get(), screenDc_, memoryDc_, bitmap_, previousBitmap_, size, size, target_)) {
        ReleaseSurface();
        return false;
    }
    size_ = size;
    return true;
}

void MonitorBadge::Show(const RECT& monitor, int number, COLORREF accent) {
    constexpr int kSize = 168;
    if (!EnsureWindow() || !EnsureSurface(kSize) || !write_) {
        return;
    }
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    write_->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        72.0f,
        L"ru-ru",
        format.GetAddressOf());
    if (format) {
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> fill;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> text;
    target_->CreateSolidColorBrush(FromAccent(accent, 0.92f), fill.GetAddressOf());
    target_->CreateSolidColorBrush(D2D1::ColorF(0.04f, 0.08f, 0.12f, 1.0f), text.GetAddressOf());

    const std::wstring label = std::to_wstring(number);
    target_->BeginDraw();
    target_->Clear(D2D1::ColorF(0, 0, 0, 0));
    target_->FillRoundedRectangle(Round(8, 8, kSize - 8, kSize - 8, 28), fill.Get());
    if (format) {
        target_->DrawText(
            label.c_str(),
            static_cast<UINT32>(label.size()),
            format.Get(),
            D2D1::RectF(8, 8, kSize - 8, kSize - 8),
            text.Get());
    }
    target_->EndDraw();

    const int x = monitor.left + ((monitor.right - monitor.left) - kSize) / 2;
    const int y = monitor.top + ((monitor.bottom - monitor.top) - kSize) / 2;
    Present(hwnd_, screenDc_, memoryDc_, x, y, kSize, kSize);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
}

void MonitorBadge::Hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

LRESULT CALLBACK MonitorBadge::Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCHITTEST) {
        return HTTRANSPARENT;
    }
    if (message == WM_MOUSEACTIVATE) {
        return MA_NOACTIVATE;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace layrix
