#pragma once

#include "EditorWindow.g.h"
#include "engine/LayoutStore.h"
#include "engine/MonitorService.h"
#include "engine/UiStrings.h"
#include "ColorPickerWindow.h"

#include <string>
#include <memory>

namespace layrix { class DesktopZoneEditor; }

namespace winrt::LayrixEditor::implementation {

struct EditorWindow : EditorWindowT<EditorWindow> {
    EditorWindow();
    ~EditorWindow();
    bool ShowSettings(bool show);
    void Present(bool settings);

private:
    void MinimizeToTray();
    void BuildRuntimeControls();
    void ReloadFromDisk();
    void Persist(std::function<void(layrix::AppSettings&)> change);
    void RebuildMonitorChips();
    void RebuildCards();
    void SyncControls();
    void SyncStartup();
    void SelectMonitor(int index);
    void AssignLayout(std::wstring const& id);
    void EditLayout(std::wstring const& id);
    void ShowCanvas(std::wstring const& name, std::wstring const& id, std::vector<layrix::NormRect> const& zones, bool isNew);
    void HideCanvas();
    void SaveCanvas();
    void DeleteCanvas();
    void DeleteLayout(std::wstring const& id);
    void RenameLayout(std::wstring const& id);
    void CommitSpacing();
    void ApplyTheme();
    void ApplyLanguage();
    void SetLanguage(layrix::UiLanguage language);
    void EnableGlassBackdrop();
    void NotifyEngine() const;
    void FlashSelected() const;
    std::wstring AssignedId() const;
    winrt::Windows::UI::Color ToUiColor(COLORREF color) const;

    struct ColorEditor {
        winrt::Microsoft::UI::Xaml::Controls::Button preview{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBox hex{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::Button palette{nullptr};
    };

    void BuildColorEditor(ColorEditor& editor, winrt::Microsoft::UI::Xaml::Controls::StackPanel const& host, bool zone);
    void SyncColorEditor(ColorEditor& editor, std::wstring const& stored, bool systemOn);
    void CommitCustomColor(bool zone, std::wstring const& text);
    void OpenColorPicker(bool zone);
    HWND NativeHandle() const;
    POINT ColorPickerAnchor(bool zone) const;
    void SyncSnapHold();
    void BeginSnapHoldCapture();
    void CancelSnapHoldCapture();
    void CommitSnapHold(layrix::KeyChord chord);
    bool HandleSnapHoldCapture(winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);

    layrix::LayoutStore store_;
    layrix::AppSettings settings_;
    std::vector<layrix::MonitorInfo> monitors_;
    std::unique_ptr<layrix::DesktopZoneEditor> desktopEditor_;
    int draftSpacing_ = 8;
    Microsoft::UI::Xaml::Controls::Slider spacingSlider_{nullptr};
    Microsoft::UI::Xaml::Controls::ToggleSwitch winArrowSwitch_{nullptr};
    ColorEditor zoneColorEditor_;
    ColorEditor accentColorEditor_;
    std::unique_ptr<layrix::ColorPickerWindow> colorPicker_;
    winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController acrylic_{nullptr};
    winrt::Microsoft::UI::Composition::SystemBackdrops::SystemBackdropConfiguration backdropConfig_{nullptr};
    int selected_ = 0;
    bool suppress_ = false;
    bool capturingSnapHold_ = false;
    bool editing_ = false;
    bool draftNew_ = false;
    std::wstring draftId_;
};

}  // namespace winrt::LayrixEditor::implementation

namespace winrt::LayrixEditor::factory_implementation {

struct EditorWindow : EditorWindowT<EditorWindow, implementation::EditorWindow> {};

}  // namespace winrt::LayrixEditor::factory_implementation
