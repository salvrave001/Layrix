#pragma once

#include "CanvasEditor.g.h"
#include "engine/LayoutModel.h"

#include <functional>
#include <vector>

namespace winrt::LayrixEditor::implementation {

struct CanvasEditor : CanvasEditorT<CanvasEditor> {
    CanvasEditor();
    void Load(winrt::hstring const& name, std::vector<layrix::NormRect> const& zones, bool canDelete = true);
    std::vector<layrix::NormRect> Zones() const;
    winrt::hstring Name();
    void SetHandlers(std::function<void()> onSave, std::function<void()> onCancel, std::function<void()> onDelete);

private:
    enum class DragMode { None, Create, Move, Resize };

    void BuildUi();
    void OnPressed(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
    void OnMoved(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
    void OnReleased();
    void Rebuild();
    void DeleteSelected();
    bool TryLocal(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args, double& x, double& y);
    int HitTest(double x, double y, int& edges);
    void SnapMove(layrix::NormRect& zone, int index) const;
    void SnapResize(layrix::NormRect& zone, int edges, int index) const;

    Microsoft::UI::Xaml::Controls::TextBox nameBox_{nullptr};
    Microsoft::UI::Xaml::Controls::Border surface_{nullptr};
    Microsoft::UI::Xaml::Controls::Canvas zoneCanvas_{nullptr};
    Microsoft::UI::Xaml::Controls::Button deleteButton_{nullptr};
    Microsoft::UI::Xaml::Controls::Button cancelButton_{nullptr};
    Microsoft::UI::Xaml::Controls::Button saveButton_{nullptr};

    std::vector<layrix::NormRect> zones_;
    int selected_ = -1;
    DragMode mode_ = DragMode::None;
    int dragIndex_ = -1;
    int dragEdges_ = 0;
    double startX_ = 0;
    double startY_ = 0;
    layrix::NormRect original_{};
    std::function<void()> onSave_;
    std::function<void()> onCancel_;
    std::function<void()> onDelete_;
};

}  // namespace winrt::LayrixEditor::implementation

namespace winrt::LayrixEditor::factory_implementation {

struct CanvasEditor : CanvasEditorT<CanvasEditor, implementation::CanvasEditor> {};

}  // namespace winrt::LayrixEditor::factory_implementation
