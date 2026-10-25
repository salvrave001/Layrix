#include "pch.h"
#include "DesktopZoneEditor.h"
#include "engine/ZoneEditing.h"
#include "engine/UiStrings.h"
#include <microsoft.ui.xaml.window.h>
#include <winrt/Windows.UI.Text.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>

namespace layrix {
namespace {
constexpr wchar_t kSurface[] = L"LayrixDesktopZoneEditor";
}

struct DesktopZoneEditor::ToolsUi {
    winrt::Microsoft::UI::Xaml::Window window{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBox name{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock counter{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Button splitX{nullptr}, splitY{nullptr}, remove{nullptr}, save{nullptr}, cancel{nullptr};
    winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController acrylic{nullptr};
    winrt::Microsoft::UI::Composition::SystemBackdrops::SystemBackdropConfiguration backdropConfig{nullptr};
    bool closing = false;
};

DesktopZoneEditor::DesktopZoneEditor() = default;
DesktopZoneEditor::~DesktopZoneEditor() { Close(); }

bool DesktopZoneEditor::Open(HWND owner, RECT work, std::wstring name, std::vector<NormRect> zones,
                             int spacing, COLORREF accent, std::function<void(bool)> finish) {
    Close();
    work_ = work;
    width_ = work.right - work.left;
    height_ = work.bottom - work.top;
    if (width_ <= 0 || height_ <= 0) return false;
    name_ = std::move(name);
    zones_ = std::move(zones);
    spacing_ = spacing;
    accent_ = accent;
    finish_ = std::move(finish);
    selected_ = zones_.empty() ? -1 : 0;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.ReleaseAndGetAddressOf())) ||
        FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(write_.ReleaseAndGetAddressOf())))) return false;
    screen_ = GetDC(nullptr);
    memory_ = CreateCompatibleDC(screen_);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width_;
    info.bmiHeader.biHeight = -height_;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    bitmap_ = CreateDIBSection(screen_, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!screen_ || !memory_ || !bitmap_) { Close(); return false; }
    previous_ = static_cast<HBITMAP>(SelectObject(memory_, bitmap_));
    auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
    RECT local{0, 0, width_, height_};
    if (FAILED(factory_->CreateDCRenderTarget(&properties, target_.ReleaseAndGetAddressOf())) ||
        FAILED(target_->BindDC(memory_, &local))) { Close(); return false; }
    for (auto className : {kSurface}) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = Proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = className;
        RegisterClassW(&wc);
    }
    overlay_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_APPWINDOW,
        kSurface, L"Layrix — зоны на рабочем столе", WS_POPUP,
        work.left, work.top, width_, height_, owner, nullptr, GetModuleHandleW(nullptr), this);
    if (!overlay_) { Close(); return false; }
    namespace xaml = winrt::Microsoft::UI::Xaml;
    namespace controls = winrt::Microsoft::UI::Xaml::Controls;
    using namespace winrt;
    using namespace xaml;
    using namespace controls;
    tools_ = std::make_unique<ToolsUi>();
    auto& ui = *tools_;
    ui.window = Window();
    ui.window.Title(hstring{Tr(L"edit_title")});
    ui.window.ExtendsContentIntoTitleBar(true);

    auto brush = [](uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return xaml::Media::SolidColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(a, r, g, b));
    };
    auto pill = [&](Button const& control) {
        control.Height(36);
        control.Padding(Thickness{14, 0, 14, 0});
        control.CornerRadius({16, 16, 16, 16});
        control.Background(brush(43, 43, 43, 230));
        control.Foreground(brush(243, 243, 243));
        control.BorderBrush(brush(255, 255, 255, 38));
        control.BorderThickness(Thickness{1, 1, 1, 1});
    };
    try {
        namespace backdrops = winrt::Microsoft::UI::Composition::SystemBackdrops;
        if (backdrops::DesktopAcrylicController::IsSupported()) {
            ui.backdropConfig = backdrops::SystemBackdropConfiguration();
            ui.backdropConfig.IsInputActive(true);
            ui.backdropConfig.Theme(backdrops::SystemBackdropTheme::Dark);
            ui.window.Activated([&ui](auto&&, WindowActivatedEventArgs const& args) {
                if (ui.backdropConfig) {
                    ui.backdropConfig.IsInputActive(
                        args.WindowActivationState() != WindowActivationState::Deactivated);
                }
            });
            ui.acrylic = backdrops::DesktopAcrylicController();
            ui.acrylic.TintColor(winrt::Windows::UI::ColorHelper::FromArgb(255, 18, 18, 18));
            ui.acrylic.TintOpacity(0.28f);
            ui.acrylic.LuminosityOpacity(0.52f);
            ui.acrylic.SetSystemBackdropConfiguration(ui.backdropConfig);
            ui.acrylic.AddSystemBackdropTarget(
                ui.window.try_as<winrt::Microsoft::UI::Composition::ICompositionSupportsSystemBackdrop>());
        } else {
            ui.window.SystemBackdrop(xaml::Media::DesktopAcrylicBackdrop());
        }
    } catch (...) {
        try {
            ui.window.SystemBackdrop(xaml::Media::DesktopAcrylicBackdrop());
        } catch (...) {
        }
    }
    Grid root;
    root.RequestedTheme(ElementTheme::Dark);
    root.Background(brush(0, 0, 0, 0));
    for (int height : {40, 72, 60}) {
        RowDefinition row;
        row.Height(GridLengthHelper::FromPixels(height));
        root.RowDefinitions().Append(row);
    }
    Grid title;
    title.Background(brush(0, 0, 0, 0));
    title.Margin(Thickness{16, 0, 54, 0});
    StackPanel heading;
    heading.Orientation(Orientation::Horizontal);
    heading.Spacing(10);
    heading.VerticalAlignment(VerticalAlignment::Center);
    FontIcon icon;
    icon.Glyph(L"\xE8A9");
    icon.FontSize(16);
    icon.Foreground(brush(229, 229, 229));
    TextBlock titleText;
    titleText.Text(hstring{Tr(L"edit_title")});
    titleText.FontSize(14);
    titleText.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    heading.Children().Append(icon);
    heading.Children().Append(titleText);
    title.Children().Append(heading);
    ui.counter = TextBlock();
    ui.counter.HorizontalAlignment(HorizontalAlignment::Right);
    ui.counter.VerticalAlignment(VerticalAlignment::Center);
    ui.counter.FontSize(12);
    ui.counter.Foreground(brush(170, 170, 170));
    title.Children().Append(ui.counter);
    root.Children().Append(title);

    Grid details;
    details.Margin(Thickness{16, 6, 16, 0});
    ColumnDefinition nameColumn;
    nameColumn.Width(GridLengthHelper::FromPixels(300));
    details.ColumnDefinitions().Append(nameColumn);
    details.ColumnDefinitions().Append(ColumnDefinition());
    details.ColumnSpacing(20);
    StackPanel nameGroup;
    nameGroup.Spacing(5);
    TextBlock nameLabel;
    nameLabel.Text(hstring{Tr(L"layout_name")});
    nameLabel.FontSize(12);
    nameLabel.Foreground(brush(190, 190, 190));
    ui.name = TextBox();
    ui.name.Text(name_);
    ui.name.PlaceholderText(hstring{Tr(L"layout_name_ph")});
    ui.name.CornerRadius({12, 12, 12, 12});
    ui.name.Background(brush(43, 43, 43, 200));
    ui.name.BorderBrush(brush(255, 255, 255, 38));
    nameGroup.Children().Append(nameLabel);
    nameGroup.Children().Append(ui.name);
    details.Children().Append(nameGroup);
    TextBlock hint;
    hint.Text(hstring{Tr(L"edit_hint")});
    hint.TextWrapping(TextWrapping::Wrap);
    hint.FontSize(13);
    hint.Foreground(brush(180, 180, 180));
    hint.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(hint, 1);
    details.Children().Append(hint);
    Grid::SetRow(details, 1);
    root.Children().Append(details);

    Grid actions;
    actions.Margin(Thickness{16, 10, 16, 14});
    actions.ColumnDefinitions().Append(ColumnDefinition());
    ColumnDefinition end;
    end.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Auto));
    actions.ColumnDefinitions().Append(end);
    actions.ColumnSpacing(16);
    StackPanel editing;
    editing.Orientation(Orientation::Horizontal);
    editing.Spacing(8);
    StackPanel finishing;
    finishing.Orientation(Orientation::Horizontal);
    finishing.Spacing(8);
    auto button = [&](const wchar_t* label, int command) {
        Button control;
        control.Content(box_value(hstring{label}));
        pill(control);
        control.Click([this, command](auto&&, auto&&) { Command(command); });
        return control;
    };
    editing.Children().Append(button(Tr(L"add_zone").c_str(), Add));
    ui.splitX = button(Tr(L"split_h").c_str(), SplitX);
    ui.splitY = button(Tr(L"split_v").c_str(), SplitY);
    ui.remove = button(Tr(L"delete_zone").c_str(), Delete);
    editing.Children().Append(ui.splitX);
    editing.Children().Append(ui.splitY);
    editing.Children().Append(ui.remove);
    ui.cancel = button(Tr(L"cancel_esc").c_str(), Cancel);
    ui.save = button(Tr(L"save").c_str(), Save);
    ToolTipService::SetToolTip(ui.save, box_value(hstring{Tr(L"save_tip")}));
    finishing.Children().Append(ui.cancel);
    finishing.Children().Append(ui.save);
    actions.Children().Append(editing);
    Grid::SetColumn(finishing, 1);
    actions.Children().Append(finishing);
    Grid::SetRow(actions, 2);
    root.Children().Append(actions);
    root.KeyDown([this](auto&&, xaml::Input::KeyRoutedEventArgs const& args) {
        if (args.Key() == winrt::Windows::System::VirtualKey::Escape ||
            (args.Key() == winrt::Windows::System::VirtualKey::S && (GetKeyState(VK_CONTROL) & 0x8000))) {
            const bool save = args.Key() == winrt::Windows::System::VirtualKey::S;
            args.Handled(true);
            Finish(save);
        }
    });
    ui.window.Content(root);
    ui.window.SetTitleBar(title);
    ui.window.Closed([this](auto&&, auto&&) {
        if (tools_ && !tools_->closing) {
            tools_->closing = true;
            Finish(false);
        }
    });
    auto native = ui.window.as<IWindowNative>();
    native->get_WindowHandle(&panel_);
    SetWindowLongPtrW(panel_, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(overlay_));
    auto appWindow = ui.window.AppWindow();
    auto titleBar = appWindow.TitleBar();
    titleBar.ButtonBackgroundColor(winrt::Windows::UI::ColorHelper::FromArgb(0, 0, 0, 0));
    titleBar.ButtonInactiveBackgroundColor(winrt::Windows::UI::ColorHelper::FromArgb(0, 0, 0, 0));
    titleBar.ButtonForegroundColor(winrt::Windows::UI::ColorHelper::FromArgb(255, 230, 230, 230));
    titleBar.ButtonHoverBackgroundColor(winrt::Windows::UI::ColorHelper::FromArgb(40, 255, 255, 255));
    titleBar.ButtonPressedBackgroundColor(winrt::Windows::UI::ColorHelper::FromArgb(70, 255, 255, 255));
    titleBar.ButtonHoverForegroundColor(winrt::Windows::UI::ColorHelper::FromArgb(255, 255, 255, 255));
    if (auto presenter = appWindow.Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>()) {
        presenter.IsResizable(false);
        presenter.IsMaximizable(false);
        presenter.IsMinimizable(false);
    }
    // AppWindow coordinates are physical pixels; XAML measures in DIPs.
    const double scale = GetDpiForWindow(owner) / 96.0;
    const int panelWidth = std::min(width_, static_cast<int>(std::lround(1000 * scale)));
    const int panelHeight = static_cast<int>(std::lround(180 * scale));
    appWindow.Resize({panelWidth, panelHeight});
    appWindow.Move({work.left + (width_ - panelWidth) / 2, work.top + static_cast<int>(16 * scale)});
    SetWindowPos(panel_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    UpdateButtons();
    Render();
    ShowWindow(overlay_, SW_SHOW);
    ui.window.Activate();
    SetForegroundWindow(panel_);
    return true;
}

std::wstring DesktopZoneEditor::Name() const {
    return tools_ ? std::wstring{tools_->name.Text()} : name_;
}

void DesktopZoneEditor::Close() {
    name_ = Name();
    mode_ = Mode::None;
    if (tools_) {
        if (tools_->acrylic) {
            tools_->acrylic.Close();
            tools_->acrylic = nullptr;
        }
        if (!tools_->closing) {
            tools_->closing = true;
            tools_->window.Close();
        }
        tools_.reset();
    }
    panel_ = nullptr;
    if (overlay_) { DestroyWindow(overlay_); overlay_ = nullptr; }
    target_.Reset();
    write_.Reset();
    factory_.Reset();
    if (memory_ && previous_) SelectObject(memory_, previous_);
    if (bitmap_) DeleteObject(bitmap_);
    if (memory_) DeleteDC(memory_);
    if (screen_) ReleaseDC(nullptr, screen_);
    screen_ = memory_ = nullptr;
    bitmap_ = previous_ = nullptr;
}

void DesktopZoneEditor::Finish(bool save) {
    if (save && zones_.empty()) return;
    auto finish = std::move(finish_);
    Close();
    if (finish) finish(save);
}

void DesktopZoneEditor::UpdateButtons() {
    if (!tools_) return;
    const bool selected = selected_ >= 0 && selected_ < static_cast<int>(zones_.size());
    tools_->splitX.IsEnabled(selected);
    tools_->splitY.IsEnabled(selected);
    tools_->remove.IsEnabled(selected);
    tools_->save.IsEnabled(!zones_.empty());
    tools_->counter.Text(winrt::hstring{Tr(L"zones_count") + std::to_wstring(zones_.size())});
}

void DesktopZoneEditor::Command(int command) {
    if (command == Save || command == Cancel) { Finish(command == Save); return; }
    if (command == Add) {
        zones_.push_back({0.25, 0.25, 0.5, 0.5});
        selected_ = static_cast<int>(zones_.size()) - 1;
    } else if (selected_ >= 0 && selected_ < static_cast<int>(zones_.size())) {
        if (command == Delete) {
            zones_.erase(zones_.begin() + selected_);
            selected_ = zones_.empty() ? -1 : std::min(selected_, static_cast<int>(zones_.size()) - 1);
        } else if (command == SplitX || command == SplitY) {
            auto zone = zones_[static_cast<size_t>(selected_)];
            if ((command == SplitX ? zone.w : zone.h) < 0.08) return;
            if (command == SplitX) zone.w /= 2; else zone.h /= 2;
            zones_[static_cast<size_t>(selected_)] = zone;
            if (command == SplitX) zone.x += zone.w; else zone.y += zone.h;
            zones_.push_back(zone);
            selected_ = static_cast<int>(zones_.size()) - 1;
        }
    }
    UpdateButtons();
    Render();
    SetFocus(overlay_);
}

double DesktopZoneEditor::X(POINT p) const { return std::clamp(p.x / static_cast<double>(width_), 0.0, 1.0); }
double DesktopZoneEditor::Y(POINT p) const { return std::clamp(p.y / static_cast<double>(height_), 0.0, 1.0); }

int DesktopZoneEditor::Hit(POINT p, int& edges) const {
    const RECT local{0, 0, width_, height_};
    for (int i = static_cast<int>(zones_.size()) - 1; i >= 0; --i) {
        RECT r = ZoneToPixels(zones_[static_cast<size_t>(i)], local, spacing_);
        InflateRect(&r, 10, 10);
        if (!PtInRect(&r, p)) continue;
        InflateRect(&r, -10, -10);
        edges = 0;
        if (std::abs(p.x - r.left) <= 10) edges |= 1;
        else if (std::abs(p.x - r.right) <= 10) edges |= 2;
        if (std::abs(p.y - r.top) <= 10) edges |= 4;
        else if (std::abs(p.y - r.bottom) <= 10) edges |= 8;
        return i;
    }
    edges = 0;
    return -1;
}

void DesktopZoneEditor::PointerDown(POINT p) {
    selected_ = Hit(p, edges_);
    original_ = zones_;
    startX_ = X(p); startY_ = Y(p);
    mode_ = selected_ < 0 ? Mode::Create : (edges_ ? Mode::Resize : Mode::Move);
    SetCapture(overlay_);
    SetFocus(overlay_);
    UpdateButtons();
    Render();
}

void DesktopZoneEditor::PointerMove(POINT p) {
    if (mode_ == Mode::None) {
        int edges = 0;
        Hit(p, edges);
        LPCWSTR cursor = IDC_CROSS;
        if ((edges & 3) && (edges & 12)) cursor = (edges == 5 || edges == 10) ? IDC_SIZENWSE : IDC_SIZENESW;
        else if (edges & 3) cursor = IDC_SIZEWE;
        else if (edges & 12) cursor = IDC_SIZENS;
        else { int ignored = 0; if (Hit(p, ignored) >= 0) cursor = IDC_SIZEALL; }
        SetCursor(LoadCursorW(nullptr, cursor));
        return;
    }
    const double x = X(p), y = Y(p);
    if (mode_ == Mode::Create) {
        if (std::abs(x - startX_) * width_ < 4 || std::abs(y - startY_) * height_ < 4) return;
        NormRect zone{std::min(x, startX_), std::min(y, startY_), std::abs(x - startX_), std::abs(y - startY_)};
        if (selected_ < 0) { zones_.push_back(zone); selected_ = static_cast<int>(zones_.size()) - 1; }
        else zones_[static_cast<size_t>(selected_)] = zone;
    } else if (mode_ == Mode::Move) {
        auto zone = original_[static_cast<size_t>(selected_)];
        zone.x = std::clamp(zone.x + x - startX_, 0.0, 1.0 - zone.w);
        zone.y = std::clamp(zone.y + y - startY_, 0.0, 1.0 - zone.h);
        zones_[static_cast<size_t>(selected_)] = zone;
    } else if (mode_ == Mode::Resize) {
        zones_ = original_;
        const auto& zone = original_[static_cast<size_t>(selected_)];
        if (edges_ & 3) ResizeZoneBoundary(zones_, original_, selected_, true, edges_ & 1,
            zone.x + ((edges_ & 1) ? 0 : zone.w) + x - startX_, 12.0 / width_);
        if (edges_ & 12) ResizeZoneBoundary(zones_, original_, selected_, false, edges_ & 4,
            zone.y + ((edges_ & 4) ? 0 : zone.h) + y - startY_, 12.0 / height_);
    }
    UpdateButtons();
    Render();
}

void DesktopZoneEditor::PointerUp() {
    if (mode_ == Mode::None) return;
    if (mode_ == Mode::Create && selected_ >= 0) {
        const auto& zone = zones_[static_cast<size_t>(selected_)];
        if (zone.w < 0.04 || zone.h < 0.04) { zones_ = original_; selected_ = -1; }
    }
    mode_ = Mode::None;
    ReleaseCapture();
    original_.clear();
    UpdateButtons();
    Render();
}

void DesktopZoneEditor::Render() {
    if (!target_ || !overlay_) return;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    target_->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), brush.GetAddressOf());
    if (!brush) return;
    const auto accent = D2D1::ColorF(GetRValue(accent_) / 255.f, GetGValue(accent_) / 255.f, GetBValue(accent_) / 255.f, 1);
    Microsoft::WRL::ComPtr<IDWriteTextFormat> text;
    write_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 28, L"ru-ru", text.GetAddressOf());
    if (text) { text->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); text->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
    target_->BeginDraw();
    // Nonzero alpha makes the entire work area interactive, including gaps.
    target_->Clear(D2D1::ColorF(0, 0, 0, 0.10f));
    const RECT local{0, 0, width_, height_};
    for (size_t i = 0; i < zones_.size(); ++i) {
        const RECT rect = ZoneToPixels(zones_[i], local, spacing_);
        const auto r = D2D1::RectF(static_cast<float>(rect.left) + 1, static_cast<float>(rect.top) + 1,
            static_cast<float>(rect.right) - 1, static_cast<float>(rect.bottom) - 1);
        const bool active = static_cast<int>(i) == selected_;
        auto fill = accent; fill.a = active ? 0.28f : 0.13f;
        brush->SetColor(fill);
        const auto rounded = D2D1::RoundedRect(r, 10, 10);
        target_->FillRoundedRectangle(rounded, brush.Get());
        brush->SetColor(accent);
        target_->DrawRoundedRectangle(rounded, brush.Get(), active ? 3.f : 1.5f);
        if (text) {
            const auto number = std::to_wstring(i + 1);
            brush->SetColor(D2D1::ColorF(1, 1, 1, 1));
            const float cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
            brush->SetColor(D2D1::ColorF(0.17f, 0.17f, 0.17f, 0.92f));
            target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - 26, cy - 24, cx + 26, cy + 24), 12, 12), brush.Get());
            brush->SetColor(D2D1::ColorF(1, 1, 1, 1));
            target_->DrawText(number.c_str(), static_cast<UINT32>(number.size()), text.Get(), D2D1::RectF(cx - 26, cy - 24, cx + 26, cy + 24), brush.Get());
        }
        if (active) {
            brush->SetColor(D2D1::ColorF(1, 1, 1, 1));
            for (auto point : {D2D1::Point2F(r.left, r.top), D2D1::Point2F(r.right, r.top),
                 D2D1::Point2F(r.left, r.bottom), D2D1::Point2F(r.right, r.bottom),
                 D2D1::Point2F((r.left + r.right) / 2, r.top), D2D1::Point2F((r.left + r.right) / 2, r.bottom),
                 D2D1::Point2F(r.left, (r.top + r.bottom) / 2), D2D1::Point2F(r.right, (r.top + r.bottom) / 2)})
                target_->FillRectangle(D2D1::RectF(point.x - 4, point.y - 4, point.x + 4, point.y + 4), brush.Get());
        }
    }
    if (FAILED(target_->EndDraw())) return;
    POINT origin{work_.left, work_.top}, source{0, 0};
    SIZE size{width_, height_};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(overlay_, screen_, &origin, &size, memory_, &source, 0, &blend, ULW_ALPHA);
}

LRESULT CALLBACK DesktopZoneEditor::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
    }
    auto* editor = reinterpret_cast<DesktopZoneEditor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return editor ? editor->Handle(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT DesktopZoneEditor::Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE: Finish(false); return 0;
    case WM_COMMAND: if (HIWORD(wp) == BN_CLICKED) Command(LOWORD(wp)); return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) Finish(false);
        else if (wp == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)) Finish(true);
        else if (wp == VK_DELETE) Command(Delete);
        return 0;
    case WM_LBUTTONDOWN: if (hwnd == overlay_) PointerDown({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}); return 0;
    case WM_MOUSEMOVE: if (hwnd == overlay_) PointerMove({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}); return 0;
    case WM_LBUTTONUP:
        if (hwnd == overlay_) { PointerMove({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}); PointerUp(); } return 0;
    case WM_CAPTURECHANGED: if (hwnd == overlay_) PointerUp(); return 0;
    case WM_DISPLAYCHANGE: Finish(false); return 0;
    case WM_ERASEBKGND: if (hwnd == overlay_) return 1; break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
} // namespace layrix
