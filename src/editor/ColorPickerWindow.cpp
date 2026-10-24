#include "pch.h"
#include "ColorPickerWindow.h"

#include "engine/LayoutModel.h"

#include <microsoft.ui.xaml.window.h>
#include <robuffer.h>
#include <wrl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using namespace winrt::Microsoft::UI::Windowing;
using namespace winrt::Windows::UI;

namespace {
constexpr int kWheelDip = 220;
constexpr int kSliderDip = 18;
constexpr int kBitmap = 220;

SolidColorBrush Fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return SolidColorBrush(ColorHelper::FromArgb(a, r, g, b));
}

uint8_t* PixelBytes(WriteableBitmap const& bitmap) {
    auto buffer = bitmap.PixelBuffer();
    ::Microsoft::WRL::ComPtr<::Windows::Storage::Streams::IBufferByteAccess> access;
    winrt::check_hresult(winrt::get_unknown(buffer)->QueryInterface(IID_PPV_ARGS(&access)));
    uint8_t* data = nullptr;
    winrt::check_hresult(access->Buffer(&data));
    return data;
}

void WriteBgra(uint8_t* pixel, float r, float g, float b, float a) {
    pixel[0] = static_cast<uint8_t>(std::lround(b * 255.0f));
    pixel[1] = static_cast<uint8_t>(std::lround(g * 255.0f));
    pixel[2] = static_cast<uint8_t>(std::lround(r * 255.0f));
    pixel[3] = static_cast<uint8_t>(std::lround(a * 255.0f));
}

std::wstring FormatUnit(float value) {
    wchar_t text[24] = {};
    swprintf_s(text, L"%.3f", value);
    return text;
}

bool ParseUnit(std::wstring const& text, float& value, float min, float max) {
    std::wstring normalized = text;
    for (wchar_t& ch : normalized) {
        if (ch == L',') {
            ch = L'.';
        }
    }
    wchar_t* end = nullptr;
    const float parsed = std::wcstof(normalized.c_str(), &end);
    if (!end || end == normalized.c_str()) {
        return false;
    }
    value = std::clamp(parsed, min, max);
    return true;
}

void StyleTab(Button const& button, bool selected) {
    button.Background(selected ? Fill(58, 58, 58) : Fill(40, 40, 40));
    button.Foreground(Fill(230, 230, 230));
    button.BorderBrush(selected ? Fill(90, 90, 90) : Fill(50, 50, 50));
    button.BorderThickness(Thickness{1, 1, 1, 1});
    button.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius(0));
    button.Padding(Thickness{0, 0, 0, 0});
}
}

namespace layrix {

ColorPickerWindow::ColorPickerWindow() = default;

ColorPickerWindow::~ColorPickerWindow() {
    Close();
}

bool ColorPickerWindow::IsOpen() const {
    return window_ != nullptr;
}

void ColorPickerWindow::Close() {
    if (!window_) {
        return;
    }
    auto closing = window_;
    window_ = nullptr;
    try {
        closing.Close();
    } catch (...) {
    }
}

HWND ColorPickerWindow::NativeHandle() const {
    HWND hwnd = nullptr;
    if (!window_) {
        return nullptr;
    }
    try {
        if (auto native = window_.try_as<IWindowNative>()) {
            native->get_WindowHandle(&hwnd);
        }
    } catch (...) {
    }
    return hwnd;
}

void ColorPickerWindow::PlaceAboveOwner(HWND owner, POINT anchor) {
    HWND hwnd = NativeHandle();
    if (!hwnd) {
        return;
    }
    if (owner) {
        SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
    }

    RECT current{};
    GetWindowRect(hwnd, &current);
    const int width = std::max(1L, current.right - current.left);
    const int height = std::max(1L, current.bottom - current.top);

    RECT work{};
    if (HMONITOR monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST)) {
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(monitor, &info)) {
            work = info.rcWork;
        }
    }
    if (work.right <= work.left) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    }

    int x = anchor.x;
    int y = anchor.y;
    if (x + width > work.right) {
        x = work.right - width;
    }
    if (y + height > work.bottom) {
        y = work.bottom - height;
    }
    if (x < work.left) {
        x = work.left;
    }
    if (y < work.top) {
        y = work.top;
    }

    SetWindowPos(hwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
}

void ColorPickerWindow::Open(HWND owner, POINT anchor, COLORREF initial, std::function<void(COLORREF)> onPreview, std::function<void(COLORREF)> onCommit) {
    onPreview_ = std::move(onPreview);
    onCommit_ = std::move(onCommit);
    lastCommit_ = initial;
    float r = GetRValue(initial) / 255.0f;
    float g = GetGValue(initial) / 255.0f;
    float b = GetBValue(initial) / 255.0f;
    RgbToHsv(r, g, b, hue_, sat_, val_);
    alpha_ = 1.0f;
    tab_ = Tab::Hsv;

    if (!window_) {
        window_ = Window();
        window_.Title(L"Цвет");
        window_.Closed([this](auto&&, auto&&) { window_ = nullptr; });
        BuildUi();
        StyleWindow();
    }

    RebuildWheel();
    RebuildSlider();
    UpdateCursor();
    UpdateSliderThumb();
    ShowTab(tab_);
    SyncFields();
    PlaceAboveOwner(owner, anchor);
    window_.Activate();
    PlaceAboveOwner(owner, anchor);
}

void ColorPickerWindow::StyleWindow() {
    window_.ExtendsContentIntoTitleBar(true);
    window_.AppWindow().ResizeClient({292, 428});
    auto titleBar = window_.AppWindow().TitleBar();
    titleBar.ButtonBackgroundColor(ColorHelper::FromArgb(0, 0, 0, 0));
    titleBar.ButtonInactiveBackgroundColor(ColorHelper::FromArgb(0, 0, 0, 0));
    titleBar.ButtonForegroundColor(ColorHelper::FromArgb(255, 220, 220, 220));
    if (auto presenter = window_.AppWindow().Presenter().try_as<OverlappedPresenter>()) {
        presenter.IsResizable(false);
        presenter.IsMaximizable(false);
        presenter.IsMinimizable(false);
    }
}

void ColorPickerWindow::BuildUi() {
    Grid root;
    root.Background(Fill(32, 32, 32));
    root.RowDefinitions().Append(RowDefinition());
    root.RowDefinitions().Append(RowDefinition());
    root.RowDefinitions().Append(RowDefinition());
    root.RowDefinitions().Append(RowDefinition());
    root.RowDefinitions().GetAt(0).Height(GridLengthHelper::FromPixels(36));
    root.RowDefinitions().GetAt(1).Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Auto));
    root.RowDefinitions().GetAt(2).Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Auto));
    root.RowDefinitions().GetAt(3).Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));

    Border titleDrag;
    titleDrag.Background(Fill(32, 32, 32));
    TextBlock title;
    title.Text(L"Цвет");
    title.Margin(Thickness{16, 0, 0, 0});
    title.VerticalAlignment(VerticalAlignment::Center);
    title.Foreground(Fill(210, 210, 210));
    titleDrag.Child(title);
    Grid::SetRow(titleDrag, 0);
    root.Children().Append(titleDrag);
    window_.SetTitleBar(titleDrag);

    Grid spectrum;
    spectrum.ColumnDefinitions().Append(ColumnDefinition());
    spectrum.ColumnDefinitions().Append(ColumnDefinition());
    spectrum.ColumnDefinitions().GetAt(0).Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    spectrum.ColumnDefinitions().GetAt(1).Width(GridLengthHelper::FromPixels(kSliderDip + 8));
    spectrum.ColumnSpacing(10);

    wheelImage_ = Image();
    wheelImage_.Width(kWheelDip);
    wheelImage_.Height(kWheelDip);
    wheelImage_.Stretch(Stretch::Uniform);
    wheelDot_ = winrt::Microsoft::UI::Xaml::Shapes::Ellipse();
    wheelDot_.Width(14);
    wheelDot_.Height(14);
    wheelDot_.Stroke(Fill(255, 255, 255));
    wheelDot_.StrokeThickness(2);
    wheelDot_.Fill(Fill(0, 0, 0, 0));
    wheelDot_.IsHitTestVisible(false);
    wheelCanvas_ = Canvas();
    wheelCanvas_.Width(kWheelDip);
    wheelCanvas_.Height(kWheelDip);
    wheelCanvas_.HorizontalAlignment(HorizontalAlignment::Center);
    wheelCanvas_.Children().Append(wheelImage_);
    wheelCanvas_.Children().Append(wheelDot_);
    wheelCanvas_.PointerPressed([this](auto&&, PointerRoutedEventArgs const& args) {
        dragging_ = true;
        wheelCanvas_.CapturePointer(args.Pointer());
        OnWheelPointer(args);
    });
    wheelCanvas_.PointerMoved([this](auto&&, PointerRoutedEventArgs const& args) {
        if (dragging_) {
            OnWheelPointer(args);
        }
    });
    wheelCanvas_.PointerReleased([this](auto&&, PointerRoutedEventArgs const& args) {
        dragging_ = false;
        wheelCanvas_.ReleasePointerCapture(args.Pointer());
        CommitIfNeeded();
    });
    wheelCanvas_.PointerCaptureLost([this](auto&&, auto&&) {
        dragging_ = false;
        CommitIfNeeded();
    });
    Grid::SetColumn(wheelCanvas_, 0);
    spectrum.Children().Append(wheelCanvas_);

    sliderImage_ = Image();
    sliderImage_.Width(kSliderDip);
    sliderImage_.Height(kWheelDip);
    sliderImage_.Stretch(Stretch::Fill);
    sliderThumb_ = winrt::Microsoft::UI::Xaml::Shapes::Ellipse();
    sliderThumb_.Width(16);
    sliderThumb_.Height(16);
    sliderThumb_.Stroke(Fill(255, 255, 255));
    sliderThumb_.StrokeThickness(2);
    sliderThumb_.Fill(Fill(0, 0, 0, 0));
    sliderThumb_.IsHitTestVisible(false);
    sliderCanvas_ = Canvas();
    sliderCanvas_.Width(kSliderDip);
    sliderCanvas_.Height(kWheelDip);
    sliderCanvas_.HorizontalAlignment(HorizontalAlignment::Right);
    sliderCanvas_.Children().Append(sliderImage_);
    sliderCanvas_.Children().Append(sliderThumb_);
    sliderCanvas_.PointerPressed([this](auto&&, PointerRoutedEventArgs const& args) {
        dragging_ = true;
        sliderCanvas_.CapturePointer(args.Pointer());
        OnSliderPointer(args);
    });
    sliderCanvas_.PointerMoved([this](auto&&, PointerRoutedEventArgs const& args) {
        if (dragging_) {
            OnSliderPointer(args);
        }
    });
    sliderCanvas_.PointerReleased([this](auto&&, PointerRoutedEventArgs const& args) {
        dragging_ = false;
        sliderCanvas_.ReleasePointerCapture(args.Pointer());
        CommitIfNeeded();
    });
    sliderCanvas_.PointerCaptureLost([this](auto&&, auto&&) {
        dragging_ = false;
        CommitIfNeeded();
    });
    Grid::SetColumn(sliderCanvas_, 1);
    spectrum.Children().Append(sliderCanvas_);
    spectrum.Margin(Thickness{16, 8, 16, 0});
    Grid::SetRow(spectrum, 1);
    root.Children().Append(spectrum);

    Grid tabs;
    tabs.ColumnDefinitions().Append(ColumnDefinition());
    tabs.ColumnDefinitions().Append(ColumnDefinition());
    tabs.ColumnDefinitions().Append(ColumnDefinition());
    tabs.Height(32);
    auto makeTab = [this, &tabs](Button& button, wchar_t const* text, Tab tab, int column) {
        button = Button();
        button.Content(winrt::box_value(winrt::hstring{text}));
        button.HorizontalAlignment(HorizontalAlignment::Stretch);
        button.VerticalAlignment(VerticalAlignment::Stretch);
        button.Click([this, tab](auto&&, auto&&) { ShowTab(tab); });
        Grid::SetColumn(button, column);
        tabs.Children().Append(button);
    };
    makeTab(rgbTab_, L"RGB", Tab::Rgb, 0);
    makeTab(hsvTab_, L"HSV", Tab::Hsv, 1);
    makeTab(hexTab_, L"Hex", Tab::Hex, 2);
    tabs.Margin(Thickness{16, 14, 16, 0});
    Grid::SetRow(tabs, 2);
    root.Children().Append(tabs);

    fields_ = StackPanel();
    fields_.Spacing(8);
    fieldA_ = MakeField();
    fieldB_ = MakeField();
    fieldC_ = MakeField();
    fieldD_ = MakeField();
    BindField(fieldA_, [this](std::wstring const& text) {
        if (tab_ == Tab::Hex) {
            COLORREF color = 0;
            if (ParseColor(text, color)) {
                SetColor(color, true);
            } else {
                SyncFields();
            }
            return;
        }
        float value = 0;
        if (!ParseUnit(text, value, 0, 1)) {
            SyncFields();
            return;
        }
        if (tab_ == Tab::Rgb) {
            float r = 0, g = 0, b = 0;
            HsvToRgb(hue_, sat_, val_, r, g, b);
            r = value;
            RgbToHsv(r, g, b, hue_, sat_, val_);
            SetHsv(hue_, sat_, val_, true);
        } else {
            SetHsv(value, sat_, val_, true);
        }
    });
    BindField(fieldB_, [this](std::wstring const& text) {
        float value = 0;
        if (!ParseUnit(text, value, 0, 1)) {
            SyncFields();
            return;
        }
        if (tab_ == Tab::Rgb) {
            float r = 0, g = 0, b = 0;
            HsvToRgb(hue_, sat_, val_, r, g, b);
            g = value;
            RgbToHsv(r, g, b, hue_, sat_, val_);
            SetHsv(hue_, sat_, val_, true);
        } else {
            SetHsv(hue_, value, val_, true);
        }
    });
    BindField(fieldC_, [this](std::wstring const& text) {
        float value = 0;
        if (!ParseUnit(text, value, 0, 1)) {
            SyncFields();
            return;
        }
        if (tab_ == Tab::Rgb) {
            float r = 0, g = 0, b = 0;
            HsvToRgb(hue_, sat_, val_, r, g, b);
            b = value;
            RgbToHsv(r, g, b, hue_, sat_, val_);
            SetHsv(hue_, sat_, val_, true);
        } else {
            SetHsv(hue_, sat_, value, true);
        }
    });
    BindField(fieldD_, [this](std::wstring const& text) {
        float value = 0;
        if (!ParseUnit(text, value, 0, 1)) {
            SyncFields();
            return;
        }
        alpha_ = value;
        if (onPreview_) {
            onPreview_(CurrentColor());
        }
        CommitIfNeeded();
    });
    fields_.Margin(Thickness{16, 12, 16, 16});
    Grid::SetRow(fields_, 3);
    root.Children().Append(fields_);
    window_.Content(root);
}

TextBox ColorPickerWindow::MakeField() {
    TextBox box;
    box.Height(32);
    box.FontFamily(FontFamily(L"Cascadia Mono, Consolas"));
    box.Background(Fill(45, 70, 110));
    box.Foreground(Fill(235, 235, 235));
    box.BorderBrush(Fill(70, 100, 145));
    box.BorderThickness(Thickness{1, 1, 1, 1});
    return box;
}

Grid ColorPickerWindow::MakeFieldRow(wchar_t const* label, TextBox const& box) {
    Grid row;
    row.ColumnDefinitions().Append(ColumnDefinition());
    row.ColumnDefinitions().Append(ColumnDefinition());
    row.ColumnDefinitions().GetAt(0).Width(GridLengthHelper::FromPixels(28));
    row.ColumnDefinitions().GetAt(1).Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    row.ColumnSpacing(10);
    TextBlock text;
    text.Text(label);
    text.VerticalAlignment(VerticalAlignment::Center);
    text.Foreground(Fill(200, 200, 200));
    Grid::SetColumn(box, 1);
    row.Children().Append(text);
    row.Children().Append(box);
    return row;
}

void ColorPickerWindow::BindField(TextBox const& box, std::function<void(std::wstring const&)> apply) {
    box.LostFocus([this, box, apply](auto&&, auto&&) {
        if (suppress_) {
            return;
        }
        apply(std::wstring(box.Text()));
    });
    box.KeyDown([this, box, apply](auto&&, KeyRoutedEventArgs const& args) {
        if (args.Key() == winrt::Windows::System::VirtualKey::Enter) {
            apply(std::wstring(box.Text()));
            args.Handled(true);
        }
    });
}

void ColorPickerWindow::ShowTab(Tab tab) {
    tab_ = tab;
    StyleTab(rgbTab_, tab == Tab::Rgb);
    StyleTab(hsvTab_, tab == Tab::Hsv);
    StyleTab(hexTab_, tab == Tab::Hex);
    fields_.Children().Clear();
    if (tab == Tab::Hex) {
        fields_.Children().Append(MakeFieldRow(L"Hex", fieldA_));
    } else if (tab == Tab::Rgb) {
        fields_.Children().Append(MakeFieldRow(L"R", fieldA_));
        fields_.Children().Append(MakeFieldRow(L"G", fieldB_));
        fields_.Children().Append(MakeFieldRow(L"B", fieldC_));
        fields_.Children().Append(MakeFieldRow(L"A", fieldD_));
    } else {
        fields_.Children().Append(MakeFieldRow(L"H", fieldA_));
        fields_.Children().Append(MakeFieldRow(L"S", fieldB_));
        fields_.Children().Append(MakeFieldRow(L"V", fieldC_));
        fields_.Children().Append(MakeFieldRow(L"A", fieldD_));
    }
    SyncFields();
}

void ColorPickerWindow::SetColor(COLORREF color, bool commit) {
    float r = GetRValue(color) / 255.0f;
    float g = GetGValue(color) / 255.0f;
    float b = GetBValue(color) / 255.0f;
    RgbToHsv(r, g, b, hue_, sat_, val_);
    SetHsv(hue_, sat_, val_, commit);
}

void ColorPickerWindow::SetHsv(float hue, float sat, float val, bool commit) {
    hue_ = std::clamp(hue, 0.0f, 1.0f);
    sat_ = std::clamp(sat, 0.0f, 1.0f);
    val_ = std::clamp(val, 0.0f, 1.0f);
    RebuildWheel();
    RebuildSlider();
    UpdateCursor();
    UpdateSliderThumb();
    SyncFields();
    const COLORREF color = CurrentColor();
    if (onPreview_) {
        onPreview_(color);
    }
    if (commit) {
        CommitIfNeeded();
    }
}

void ColorPickerWindow::OnWheelPointer(PointerRoutedEventArgs const& args) {
    const auto point = args.GetCurrentPoint(wheelCanvas_).Position();
    const double radius = (kWheelDip / 2.0) - 2.0;
    const double dx = point.X - (kWheelDip / 2.0);
    const double dy = point.Y - (kWheelDip / 2.0);
    const double distance = std::hypot(dx, dy);
    double hue = std::atan2(dy, dx) / (2.0 * 3.14159265358979323846);
    if (hue < 0) {
        hue += 1.0;
    }
    SetHsv(static_cast<float>(hue), static_cast<float>(std::clamp(distance / radius, 0.0, 1.0)), val_, false);
}

void ColorPickerWindow::OnSliderPointer(PointerRoutedEventArgs const& args) {
    const double y = args.GetCurrentPoint(sliderCanvas_).Position().Y;
    const float value = static_cast<float>(std::clamp(1.0 - (y / kWheelDip), 0.0, 1.0));
    SetHsv(hue_, sat_, value, false);
}

void ColorPickerWindow::CommitIfNeeded() {
    const COLORREF color = CurrentColor();
    if (color == lastCommit_) {
        return;
    }
    lastCommit_ = color;
    if (onCommit_) {
        onCommit_(color);
    }
}

void ColorPickerWindow::RebuildWheel() {
    WriteableBitmap bitmap(kBitmap, kBitmap);
    uint8_t* pixels = PixelBytes(bitmap);
    const float cx = (kBitmap - 1) * 0.5f;
    const float radius = cx - 1.0f;
    for (int y = 0; y < kBitmap; ++y) {
        for (int x = 0; x < kBitmap; ++x) {
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cx;
            const float distance = std::hypot(dx, dy);
            uint8_t* pixel = pixels + (y * kBitmap + x) * 4;
            if (distance > radius) {
                pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
                continue;
            }
            float hue = std::atan2(dy, dx) / (2.0f * 3.14159265f);
            if (hue < 0) {
                hue += 1.0f;
            }
            const float sat = distance / radius;
            float r = 0, g = 0, b = 0;
            HsvToRgb(hue, sat, val_, r, g, b);
            const float edge = std::clamp((radius - distance) * 2.0f, 0.0f, 1.0f);
            WriteBgra(pixel, r, g, b, edge);
        }
    }
    bitmap.Invalidate();
    wheelImage_.Source(bitmap);
}

void ColorPickerWindow::RebuildSlider() {
    WriteableBitmap bitmap(kSliderDip, kBitmap);
    uint8_t* pixels = PixelBytes(bitmap);
    for (int y = 0; y < kBitmap; ++y) {
        const float value = 1.0f - static_cast<float>(y) / static_cast<float>(kBitmap - 1);
        float r = 0, g = 0, b = 0;
        HsvToRgb(hue_, sat_, value, r, g, b);
        for (int x = 0; x < kSliderDip; ++x) {
            WriteBgra(pixels + (y * kSliderDip + x) * 4, r, g, b, 1);
        }
    }
    bitmap.Invalidate();
    sliderImage_.Source(bitmap);
}

void ColorPickerWindow::UpdateCursor() {
    const double radius = (kWheelDip / 2.0) - 2.0;
    const double angle = hue_ * 2.0 * 3.14159265358979323846;
    const double x = (kWheelDip / 2.0) + std::cos(angle) * sat_ * radius - 7.0;
    const double y = (kWheelDip / 2.0) + std::sin(angle) * sat_ * radius - 7.0;
    Canvas::SetLeft(wheelDot_, x);
    Canvas::SetTop(wheelDot_, y);
}

void ColorPickerWindow::UpdateSliderThumb() {
    Canvas::SetLeft(sliderThumb_, (kSliderDip - 16) / 2.0);
    Canvas::SetTop(sliderThumb_, (1.0 - val_) * kWheelDip - 8.0);
}

void ColorPickerWindow::SyncFields() {
    suppress_ = true;
    float r = 0, g = 0, b = 0;
    HsvToRgb(hue_, sat_, val_, r, g, b);
    if (tab_ == Tab::Hex) {
        fieldA_.Text(ColorToHex(CurrentColor()));
    } else if (tab_ == Tab::Rgb) {
        fieldA_.Text(FormatUnit(r));
        fieldB_.Text(FormatUnit(g));
        fieldC_.Text(FormatUnit(b));
        fieldD_.Text(FormatUnit(alpha_));
    } else {
        fieldA_.Text(FormatUnit(hue_));
        fieldB_.Text(FormatUnit(sat_));
        fieldC_.Text(FormatUnit(val_));
        fieldD_.Text(FormatUnit(alpha_));
    }
    suppress_ = false;
}

COLORREF ColorPickerWindow::CurrentColor() const {
    float r = 0, g = 0, b = 0;
    HsvToRgb(hue_, sat_, val_, r, g, b);
    return RGB(
        static_cast<BYTE>(std::lround(r * 255.0f)),
        static_cast<BYTE>(std::lround(g * 255.0f)),
        static_cast<BYTE>(std::lround(b * 255.0f)));
}

void ColorPickerWindow::RgbToHsv(float r, float g, float b, float& h, float& s, float& v) {
    const float max = std::max({r, g, b});
    const float min = std::min({r, g, b});
    const float delta = max - min;
    v = max;
    s = max <= 0 ? 0 : delta / max;
    if (delta <= 0) {
        h = 0;
        return;
    }
    if (max == r) {
        h = (g - b) / delta;
    } else if (max == g) {
        h = 2.0f + (b - r) / delta;
    } else {
        h = 4.0f + (r - g) / delta;
    }
    h /= 6.0f;
    if (h < 0) {
        h += 1.0f;
    }
}

void ColorPickerWindow::HsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    if (s <= 0) {
        r = g = b = v;
        return;
    }
    h = std::fmod(h, 1.0f);
    if (h < 0) {
        h += 1.0f;
    }
    const float sector = h * 6.0f;
    const int index = static_cast<int>(std::floor(sector)) % 6;
    const float f = sector - std::floor(sector);
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - s * f);
    const float t = v * (1.0f - s * (1.0f - f));
    switch (index) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
}

}  // namespace layrix
