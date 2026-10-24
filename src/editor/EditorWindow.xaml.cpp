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
SolidColorBrush Paint(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return SolidColorBrush(ColorHelper::FromArgb(a, r, g, b));
}

Color Mix(Color left, Color right, float amount) {
    const auto channel = [amount](uint8_t from, uint8_t to) {
        return static_cast<uint8_t>(from + (static_cast<float>(to) - from) * amount);
    };
    return ColorHelper::FromArgb(255, channel(left.R, right.R), channel(left.G, right.G), channel(left.B, right.B));
}

int ZoneDisplayToIndex(layrix::ZoneDisplayMode mode) {
    switch (mode) {
    case layrix::ZoneDisplayMode::Hidden:
        return 0;
    case layrix::ZoneDisplayMode::OnDesktop:
        return 2;
    case layrix::ZoneDisplayMode::OnTop:
    default:
        return 1;
    }
}

layrix::ZoneDisplayMode ZoneDisplayFromIndex(int index) {
    if (index == 0) {
        return layrix::ZoneDisplayMode::Hidden;
    }
    if (index == 2) {
        return layrix::ZoneDisplayMode::OnDesktop;
    }
    return layrix::ZoneDisplayMode::OnTop;
}

void PutResource(ResourceDictionary const& resources, wchar_t const* key, winrt::Windows::Foundation::IInspectable const& value) {
    const auto boxed = box_value(hstring{key});
    if (resources.HasKey(boxed)) {
        resources.Remove(boxed);
    }
    resources.Insert(boxed, value);
}
}

namespace winrt::LayrixEditor::implementation {

EditorWindow::~EditorWindow() {
    if (colorPicker_) {
        colorPicker_->Close();
    }
    if (acrylic_) {
        acrylic_.Close();
        acrylic_ = nullptr;
    }
}

EditorWindow::EditorWindow() : store_(layrix::LayoutStore::BesideExecutable()) {
    InitializeComponent();
    EnableGlassBackdrop();
    BuildRuntimeControls();
    ExtendsContentIntoTitleBar(true);
    SetTitleBar(TitleDrag());
    AppWindow().Resize({1100, 740});
    auto titleBar = AppWindow().TitleBar();
    titleBar.ButtonBackgroundColor(ColorHelper::FromArgb(0, 0, 0, 0));
    titleBar.ButtonInactiveBackgroundColor(ColorHelper::FromArgb(0, 0, 0, 0));
    titleBar.ButtonForegroundColor(ColorHelper::FromArgb(255, 230, 230, 230));
    titleBar.ButtonHoverBackgroundColor(ColorHelper::FromArgb(40, 255, 255, 255));
    titleBar.ButtonPressedBackgroundColor(ColorHelper::FromArgb(70, 255, 255, 255));

    desktopEditor_ = std::make_unique<layrix::DesktopZoneEditor>();
    colorPicker_ = std::make_unique<layrix::ColorPickerWindow>();

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
    ZoneDisplayCombo().SelectionChanged([this](auto&&, auto&&) {
        if (suppress_) return;
        const auto mode = ZoneDisplayFromIndex(ZoneDisplayCombo().SelectedIndex());
        if (mode == settings_.zoneDisplay) {
            return;
        }
        Persist([mode](layrix::AppSettings& settings) { settings.zoneDisplay = mode; });
    });
    SnapHoldButton().Click([this](auto&&, auto&&) {
        if (capturingSnapHold_) {
            CancelSnapHoldCapture();
        } else {
            BeginSnapHoldCapture();
        }
    });
    SnapHoldReset().Click([this](auto&&, auto&&) {
        CommitSnapHold(layrix::DefaultSnapHold());
    });
    SnapHoldButton().KeyDown([this](auto&&, KeyRoutedEventArgs const& args) {
        HandleSnapHoldCapture(args);
    });
    ZoneColorSystemToggle().Toggled([this](auto&&, auto&&) {
        if (suppress_) return;
        const bool system = ZoneColorSystemToggle().IsOn();
        Persist([system](layrix::AppSettings& settings) {
            settings.highlightColor = system ? std::wstring(layrix::kSystemColor)
                : layrix::ColorToHex(layrix::ResolveColor(settings.highlightColor));
        });
    });
    AccentColorSystemToggle().Toggled([this](auto&&, auto&&) {
        if (suppress_) return;
        const bool system = AccentColorSystemToggle().IsOn();
        Persist([system](layrix::AppSettings& settings) {
            settings.accentColor = system ? std::wstring(layrix::kSystemColor)
                : layrix::ColorToHex(layrix::ResolveColor(settings.accentColor));
        });
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
    CreateButton().Click([this](auto&&, auto&&) {
        ShowCanvas(layrix::NextCustomName(settings_), L"", {}, true);
    });
    Root().KeyDown([this](auto&&, KeyRoutedEventArgs const& args) {
        if (HandleSnapHoldCapture(args)) {
            return;
        }
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

    winArrowSwitch_ = ToggleSwitch();
    winArrowSwitch_.MinWidth(0);
    winArrowSwitch_.OnContent(box_value(hstring{}));
    winArrowSwitch_.OffContent(box_value(hstring{}));

    auto children = ControlsHost().Children();
    if (auto spacingGroup = children.GetAt(0).try_as<StackPanel>()) {
        spacingGroup.Children().InsertAt(1, spacingSlider_);
    }
    if (auto hotkeyGroup = children.GetAt(1).try_as<StackPanel>()) {
        hotkeyGroup.Children().Append(winArrowSwitch_);
    }
    BuildColorEditor(zoneColorEditor_, ZoneColorHost(), true);
    BuildColorEditor(accentColorEditor_, AccentColorHost(), false);
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
        chip.CornerRadius({10, 10, 10, 10});
        const bool active = index == selected_;
        chip.Background(active ? Paint(48, 48, 48, 220) : Paint(36, 36, 36, 170));
        chip.Foreground(Paint(243, 243, 243));
        chip.BorderBrush(active ? Paint(230, 230, 230, 180) : Paint(255, 255, 255, 36));
        const double border = active ? 1.5 : 1.0;
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
        view->SetAccent(ToUiColor(layrix::ResolveColor(settings_.accentColor)));
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
    ZoneDisplayCombo().SelectedIndex(ZoneDisplayToIndex(settings_.zoneDisplay));
    ZoneDisplayHint().Text(hstring{layrix::ZoneDisplayHint(settings_.zoneDisplay)});
    SyncSnapHold();
    const bool zoneSystem = layrix::IsSystemColorToken(settings_.highlightColor);
    const bool accentSystem = layrix::IsSystemColorToken(settings_.accentColor);
    ZoneColorSystemToggle().IsOn(zoneSystem);
    AccentColorSystemToggle().IsOn(accentSystem);
    SyncColorEditor(zoneColorEditor_, settings_.highlightColor, zoneSystem);
    SyncColorEditor(accentColorEditor_, settings_.accentColor, accentSystem);
    suppress_ = false;
    ApplyTheme();
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
    if (!show && colorPicker_) {
        colorPicker_->Close();
    }
    if (!show) {
        CancelSnapHoldCapture();
    }
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
    const COLORREF color = layrix::ResolveColor(settings_.highlightColor);
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

winrt::Windows::UI::Color EditorWindow::ToUiColor(COLORREF color) const {
    return ColorHelper::FromArgb(255, GetRValue(color), GetGValue(color), GetBValue(color));
}

void EditorWindow::EnableGlassBackdrop() {
    using namespace Microsoft::UI::Composition::SystemBackdrops;
    try {
        if (DesktopAcrylicController::IsSupported()) {
            backdropConfig_ = SystemBackdropConfiguration();
            backdropConfig_.IsInputActive(true);
            backdropConfig_.Theme(SystemBackdropTheme::Dark);
            Activated([this](auto&&, WindowActivatedEventArgs const& args) {
                if (backdropConfig_) {
                    backdropConfig_.IsInputActive(
                        args.WindowActivationState() != WindowActivationState::Deactivated);
                }
            });
            acrylic_ = DesktopAcrylicController();
            acrylic_.TintColor(ColorHelper::FromArgb(255, 18, 18, 18));
            acrylic_.TintOpacity(0.28f);
            acrylic_.LuminosityOpacity(0.52f);
            acrylic_.SetSystemBackdropConfiguration(backdropConfig_);
            acrylic_.AddSystemBackdropTarget(
                this->try_as<Microsoft::UI::Composition::ICompositionSupportsSystemBackdrop>());
            Root().Background(SolidColorBrush(ColorHelper::FromArgb(0, 0, 0, 0)));
            return;
        }
    } catch (...) {
    }
    try {
        SystemBackdrop(Microsoft::UI::Xaml::Media::DesktopAcrylicBackdrop());
        Root().Background(SolidColorBrush(ColorHelper::FromArgb(36, 16, 16, 16)));
    } catch (...) {
        Root().Background(SolidColorBrush(ColorHelper::FromArgb(220, 20, 20, 20)));
    }
}

void EditorWindow::ApplyTheme() {
    const Color accent = ToUiColor(layrix::ResolveColor(settings_.accentColor));
    TitleIcon().Foreground(Paint(229, 229, 229));
    try {
        auto titleBar = AppWindow().TitleBar();
        titleBar.ButtonBackgroundColor(ColorHelper::FromArgb(0, 0, 0, 0));
        titleBar.ButtonInactiveBackgroundColor(ColorHelper::FromArgb(0, 0, 0, 0));
        titleBar.ButtonForegroundColor(ColorHelper::FromArgb(255, 230, 230, 230));
        titleBar.ButtonHoverBackgroundColor(ColorHelper::FromArgb(40, 255, 255, 255));
        titleBar.ButtonPressedBackgroundColor(ColorHelper::FromArgb(70, 255, 255, 255));
        titleBar.ButtonHoverForegroundColor(ColorHelper::FromArgb(255, 255, 255, 255));
    } catch (...) {
    }

    const Color white = ColorHelper::FromArgb(255, 255, 255, 255);
    const Color black = ColorHelper::FromArgb(255, 0, 0, 0);
    const Color light1 = Mix(accent, white, 0.18f);
    const Color light2 = Mix(accent, white, 0.36f);
    const Color light3 = Mix(accent, white, 0.54f);
    const Color dark1 = Mix(accent, black, 0.18f);
    const Color dark2 = Mix(accent, black, 0.36f);
    const Color dark3 = Mix(accent, black, 0.54f);
    try {
        auto resources = Application::Current().Resources();
        PutResource(resources, L"SystemAccentColor", box_value(accent));
        PutResource(resources, L"SystemAccentColorLight1", box_value(light1));
        PutResource(resources, L"SystemAccentColorLight2", box_value(light2));
        PutResource(resources, L"SystemAccentColorLight3", box_value(light3));
        PutResource(resources, L"SystemAccentColorDark1", box_value(dark1));
        PutResource(resources, L"SystemAccentColorDark2", box_value(dark2));
        PutResource(resources, L"SystemAccentColorDark3", box_value(dark3));
        PutResource(resources, L"AccentFillColorDefaultBrush", SolidColorBrush(accent));
        PutResource(resources, L"AccentFillColorSecondaryBrush", SolidColorBrush(light1));
        PutResource(resources, L"AccentFillColorTertiaryBrush", SolidColorBrush(light2));
        PutResource(resources, L"AccentTextFillColorPrimaryBrush", SolidColorBrush(accent));
    } catch (...) {
    }
}

void EditorWindow::BuildColorEditor(ColorEditor& editor, StackPanel const& host, bool zone) {
    StackPanel row;
    row.Orientation(Orientation::Horizontal);
    row.Spacing(10);
    row.VerticalAlignment(VerticalAlignment::Center);

    editor.preview = Button();
    editor.preview.Width(36);
    editor.preview.Height(36);
    editor.preview.Padding(Thickness{0, 0, 0, 0});
    editor.preview.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius(6.0));
    editor.preview.BorderBrush(Paint(80, 80, 80));
    editor.preview.BorderThickness(Thickness{1, 1, 1, 1});
    editor.preview.Click([this, zone](auto&&, auto&&) { OpenColorPicker(zone); });
    row.Children().Append(editor.preview);

    editor.hex = TextBox();
    editor.hex.Width(140);
    editor.hex.PlaceholderText(L"#0078D4");
    editor.hex.LostFocus([this, zone](auto&&, auto&&) {
        CommitCustomColor(zone, std::wstring(zone ? zoneColorEditor_.hex.Text() : accentColorEditor_.hex.Text()));
    });
    editor.hex.KeyDown([this, zone](auto&&, KeyRoutedEventArgs const& args) {
        if (args.Key() == Windows::System::VirtualKey::Enter) {
            CommitCustomColor(zone, std::wstring(zone ? zoneColorEditor_.hex.Text() : accentColorEditor_.hex.Text()));
            args.Handled(true);
        }
    });
    row.Children().Append(editor.hex);

    editor.palette = Button();
    editor.palette.Content(box_value(hstring{L"Палитра"}));
    editor.palette.Height(36);
    editor.palette.Padding(Thickness{14, 0, 14, 0});
    editor.palette.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius(16.0));
    editor.palette.Background(Paint(43, 43, 43, 230));
    editor.palette.Foreground(Paint(243, 243, 243));
    editor.palette.BorderBrush(Paint(255, 255, 255, 38));
    editor.palette.BorderThickness(Thickness{1, 1, 1, 1});
    editor.palette.Click([this, zone](auto&&, auto&&) { OpenColorPicker(zone); });
    row.Children().Append(editor.palette);
    host.Children().Append(row);
}

void EditorWindow::SyncColorEditor(ColorEditor& editor, std::wstring const& stored, bool systemOn) {
    const COLORREF color = layrix::ResolveColor(stored);
    if (editor.preview) {
        editor.preview.Background(SolidColorBrush(ToUiColor(color)));
        editor.preview.IsEnabled(true);
    }
    if (editor.hex) {
        editor.hex.Text(layrix::ColorToHex(color));
        editor.hex.IsEnabled(!systemOn);
    }
}

HWND EditorWindow::NativeHandle() const {
    HWND hwnd = nullptr;
    try {
        auto native = this->try_as<IWindowNative>();
        if (native) {
            native->get_WindowHandle(&hwnd);
        }
    } catch (...) {
    }
    return hwnd;
}

POINT EditorWindow::ColorPickerAnchor(bool zone) const {
    const auto& editor = zone ? zoneColorEditor_ : accentColorEditor_;
    FrameworkElement target = editor.palette ? editor.palette.try_as<FrameworkElement>() : nullptr;
    if (!target && editor.preview) {
        target = editor.preview.try_as<FrameworkElement>();
    }

    POINT screen{};
    HWND owner = NativeHandle();
    if (owner) {
        POINT origin{0, 0};
        ClientToScreen(owner, &origin);
        screen = origin;
    }

    if (!target) {
        if (owner) {
            RECT bounds{};
            GetWindowRect(owner, &bounds);
            screen.x = bounds.left + 48;
            screen.y = bounds.top + 72;
        }
        return screen;
    }

    try {
        const auto transform = target.TransformToVisual(nullptr);
        const auto point = transform.TransformPoint({
            static_cast<float>(target.ActualWidth()),
            0.0f,
        });
        float scale = 1.0f;
        if (auto root = target.XamlRoot()) {
            scale = static_cast<float>(root.RasterizationScale());
        }
        POINT origin{0, 0};
        if (owner) {
            ClientToScreen(owner, &origin);
        }
        screen.x = origin.x + static_cast<int>(std::lround(point.X * scale)) + 12;
        screen.y = origin.y + static_cast<int>(std::lround(point.Y * scale));
    } catch (...) {
    }
    return screen;
}

void EditorWindow::OpenColorPicker(bool zone) {
    const std::wstring stored = zone ? settings_.highlightColor : settings_.accentColor;
    const COLORREF current = layrix::ResolveColor(stored);
    colorPicker_->Open(
        NativeHandle(),
        ColorPickerAnchor(zone),
        current,
        [this, zone](COLORREF color) {
            auto& editor = zone ? zoneColorEditor_ : accentColorEditor_;
            if (editor.preview) {
                editor.preview.Background(SolidColorBrush(ToUiColor(color)));
            }
            if (editor.hex) {
                editor.hex.Text(layrix::ColorToHex(color));
            }
        },
        [this, zone](COLORREF color) {
            CommitCustomColor(zone, layrix::ColorToHex(color));
        });
}

void EditorWindow::SyncSnapHold() {
    capturingSnapHold_ = false;
    SnapHoldButton().Content(box_value(hstring{layrix::FormatKeyChord(settings_.snapHold)}));
    ZonesSnapHint().Text(hstring{layrix::FormatSnapHint(settings_.snapHold)});
    FooterHint().Text(hstring{layrix::FormatSnapFooter(settings_.snapHold)});
    SnapHoldHint().Text(L"Нажмите кнопку с комбинацией, затем клавишу или сочетание. Esc отменяет запись.");
}

void EditorWindow::BeginSnapHoldCapture() {
    capturingSnapHold_ = true;
    SnapHoldButton().Content(box_value(hstring{L"Нажмите клавишу…"}));
    SnapHoldHint().Text(L"Удерживайте модификаторы и нажмите клавишу. Esc — отмена.");
    SnapHoldButton().Focus(FocusState::Programmatic);
}

void EditorWindow::CancelSnapHoldCapture() {
    if (!capturingSnapHold_) {
        return;
    }
    capturingSnapHold_ = false;
    SyncSnapHold();
}

void EditorWindow::CommitSnapHold(layrix::KeyChord chord) {
    capturingSnapHold_ = false;
    if (layrix::KeyChordEmpty(chord)) {
        chord = layrix::DefaultSnapHold();
    }
    if (layrix::KeyChordEqual(chord, settings_.snapHold)) {
        SyncSnapHold();
        return;
    }
    Persist([chord](layrix::AppSettings& settings) { settings.snapHold = chord; });
}

bool EditorWindow::HandleSnapHoldCapture(KeyRoutedEventArgs const& args) {
    if (!capturingSnapHold_) {
        return false;
    }
    const auto key = args.Key();
    if (key == Windows::System::VirtualKey::Escape) {
        CancelSnapHoldCapture();
        args.Handled(true);
        return true;
    }
    if (key == Windows::System::VirtualKey::Tab || key == Windows::System::VirtualKey::Enter) {
        return true;
    }
    const auto chord = layrix::KeyChordFromVirtualKey(static_cast<UINT>(key));
    if (layrix::KeyChordEmpty(chord)) {
        args.Handled(true);
        return true;
    }
    CommitSnapHold(chord);
    args.Handled(true);
    return true;
}

void EditorWindow::CommitCustomColor(bool zone, std::wstring const& text) {
    if (suppress_) {
        return;
    }
    COLORREF color = 0;
    if (!layrix::ParseColor(text, color)) {
        suppress_ = true;
        if (zone) {
            SyncColorEditor(zoneColorEditor_, settings_.highlightColor, layrix::IsSystemColorToken(settings_.highlightColor));
        } else {
            SyncColorEditor(accentColorEditor_, settings_.accentColor, layrix::IsSystemColorToken(settings_.accentColor));
        }
        suppress_ = false;
        return;
    }
    const std::wstring hex = layrix::ColorToHex(color);
    const std::wstring current = zone ? settings_.highlightColor : settings_.accentColor;
    if (!layrix::IsSystemColorToken(current) && _wcsicmp(hex.c_str(), current.c_str()) == 0) {
        return;
    }
    Persist([zone, hex](layrix::AppSettings& settings) {
        if (zone) {
            settings.highlightColor = hex;
        } else {
            settings.accentColor = hex;
        }
    });
}

}  // namespace winrt::LayrixEditor::implementation
