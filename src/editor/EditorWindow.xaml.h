#pragma once

#include "EditorWindow.g.h"
#include "engine/LayoutStore.h"
#include "engine/MonitorService.h"

#include <string>
#include <memory>

namespace layrix { class DesktopZoneEditor; }

namespace winrt::LayrixEditor::implementation {

struct EditorWindow : EditorWindowT<EditorWindow> {
    EditorWindow();
    ~EditorWindow();
    bool ShowSettings(bool show);

private:
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
    void CommitSpacing();
    void CommitColor();
    void NotifyEngine() const;
    void FlashSelected() const;
    std::wstring AssignedId() const;

    layrix::LayoutStore store_;
    layrix::AppSettings settings_;
    std::vector<layrix::MonitorInfo> monitors_;
    std::unique_ptr<layrix::DesktopZoneEditor> desktopEditor_;
    int draftSpacing_ = 8;
    Microsoft::UI::Xaml::Controls::Slider spacingSlider_{nullptr};
    Microsoft::UI::Xaml::Controls::TextBox colorBox_{nullptr};
    Microsoft::UI::Xaml::Controls::ToggleSwitch winArrowSwitch_{nullptr};
    int selected_ = 0;
    bool suppress_ = false;
    bool editing_ = false;
    bool draftNew_ = false;
    std::wstring draftId_;
};

}  // namespace winrt::LayrixEditor::implementation

namespace winrt::LayrixEditor::factory_implementation {

struct EditorWindow : EditorWindowT<EditorWindow, implementation::EditorWindow> {};

}  // namespace winrt::LayrixEditor::factory_implementation
