#include "pch.h"
#include "CanvasEditor.xaml.h"
#include "engine/UiStrings.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Input;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::System;
using namespace Windows::UI;

namespace {
SolidColorBrush Paint(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return SolidColorBrush(ColorHelper::FromArgb(a, r, g, b));
}

double SnapTo(double value, const std::vector<double>& targets) {
    double best = value;
    double bestDistance = 0.015;
    for (double target : targets) {
        const double distance = std::abs(value - target);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = target;
        }
    }
    return best;
}
}

namespace winrt::LayrixEditor::implementation {

void CanvasEditor::BuildUi() {
    Grid root;
    root.RowDefinitions().Append(RowDefinition());
    root.RowDefinitions().Append(RowDefinition());
    root.RowDefinitions().Append(RowDefinition());
    root.RowDefinitions().GetAt(0).Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Auto));
    root.RowDefinitions().GetAt(1).Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    root.RowDefinitions().GetAt(2).Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Auto));
    root.RowSpacing(12);

    nameBox_ = TextBox();
    nameBox_.PlaceholderText(hstring{layrix::Tr(L"layout_name_ph")});
    TextBlock nameLabel;
    nameLabel.Text(hstring{layrix::Tr(L"name")});
    nameLabel.Margin(Thickness{0, 0, 0, 4});
    nameLabel.Foreground(Paint(255, 255, 255, 217));
    TextBlock hint;
    hint.Text(hstring{layrix::Tr(L"canvas_hint")});
    hint.Opacity(0.7);
    hint.TextWrapping(TextWrapping::Wrap);
    hint.Foreground(Paint(255, 255, 255));
    hint.VerticalAlignment(VerticalAlignment::Bottom);
    Grid top;
    top.ColumnDefinitions().Append(ColumnDefinition());
    top.ColumnDefinitions().Append(ColumnDefinition());
    top.ColumnDefinitions().GetAt(0).Width(GridLengthHelper::FromPixels(280));
    top.ColumnDefinitions().GetAt(1).Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    top.ColumnSpacing(12);
    StackPanel nameStack;
    nameStack.Children().Append(nameLabel);
    nameStack.Children().Append(nameBox_);
    top.Children().Append(nameStack);
    Grid::SetColumn(hint, 1);
    top.Children().Append(hint);
    root.Children().Append(top);

    zoneCanvas_ = Canvas();
    zoneCanvas_.IsHitTestVisible(false);
    surface_ = Border();
    surface_.Background(Paint(28, 28, 28, 150));
    surface_.BorderBrush(Paint(255, 255, 255, 40));
    surface_.BorderThickness(Thickness{1, 1, 1, 1});
    surface_.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius(16.0));
    surface_.Child(zoneCanvas_);
    Grid::SetRow(surface_, 1);
    root.Children().Append(surface_);

    StackPanel buttons;
    buttons.Orientation(Orientation::Horizontal);
    buttons.HorizontalAlignment(HorizontalAlignment::Right);
    buttons.Spacing(8);
    auto stylePill = [](Button const& button) {
        button.Height(36);
        button.Background(Paint(43, 43, 43, 230));
        button.Foreground(Paint(243, 243, 243));
        button.BorderBrush(Paint(255, 255, 255, 38));
        button.BorderThickness(Thickness{1, 1, 1, 1});
        button.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius(16.0));
    };
    deleteButton_ = Button();
    deleteButton_.Content(box_value(hstring{layrix::Tr(L"delete_layout")}));
    deleteButton_.Padding(Thickness{14, 0, 14, 0});
    stylePill(deleteButton_);
    cancelButton_ = Button();
    cancelButton_.Content(box_value(hstring{layrix::Tr(L"cancel")}));
    cancelButton_.Padding(Thickness{14, 0, 14, 0});
    stylePill(cancelButton_);
    saveButton_ = Button();
    saveButton_.Content(box_value(hstring{layrix::Tr(L"save")}));
    saveButton_.Padding(Thickness{16, 0, 16, 0});
    stylePill(saveButton_);
    buttons.Children().Append(deleteButton_);
    buttons.Children().Append(cancelButton_);
    buttons.Children().Append(saveButton_);
    StackPanel tools;
    tools.Orientation(Orientation::Horizontal);
    tools.Spacing(8);
    Button add;
    add.Content(box_value(hstring{layrix::Tr(L"add_zone")}));
    add.Padding(Thickness{14, 0, 14, 0});
    stylePill(add);
    add.Click([this](auto&&, auto&&) {
        zones_.push_back({0.25, 0.25, 0.5, 0.5});
        selected_ = static_cast<int>(zones_.size()) - 1;
        Rebuild();
        surface_.Focus(FocusState::Programmatic);
    });
    tools.Children().Append(add);
    for (bool vertical : {true, false}) {
        Button split;
        split.Content(box_value(hstring{layrix::Tr(vertical ? L"split_h" : L"split_v")}));
        split.Padding(Thickness{14, 0, 14, 0});
        stylePill(split);
        split.Click([this, vertical](auto&&, auto&&) {
            if (selected_ < 0 || selected_ >= static_cast<int>(zones_.size())) return;
            auto zone = zones_[static_cast<size_t>(selected_)];
            if ((vertical ? zone.w : zone.h) < 0.1) return;
            if (vertical) zone.w /= 2; else zone.h /= 2;
            zones_[static_cast<size_t>(selected_)] = zone;
            if (vertical) zone.x += zone.w; else zone.y += zone.h;
            zones_.push_back(zone);
            selected_ = static_cast<int>(zones_.size()) - 1;
            Rebuild();
            surface_.Focus(FocusState::Programmatic);
        });
        tools.Children().Append(split);
    }
    Grid footer;
    footer.ColumnDefinitions().Append(ColumnDefinition());
    footer.ColumnDefinitions().Append(ColumnDefinition());
    footer.ColumnDefinitions().GetAt(1).Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Auto));
    footer.Children().Append(tools);
    Grid::SetColumn(buttons, 1);
    footer.Children().Append(buttons);
    Grid::SetRow(footer, 2);
    root.Children().Append(footer);

    Content(root);
}

CanvasEditor::CanvasEditor() {
    InitializeComponent();
    BuildUi();
    surface_.SizeChanged([this](auto&&, auto&&) { Rebuild(); });
    surface_.IsTabStop(true);
    surface_.PointerPressed([this](IInspectable const&, PointerRoutedEventArgs const& args) { OnPressed(args); });
    surface_.PointerMoved([this](IInspectable const&, PointerRoutedEventArgs const& args) { OnMoved(args); });
    surface_.PointerReleased([this](IInspectable const&, PointerRoutedEventArgs const&) { OnReleased(); });
    surface_.PointerCaptureLost([this](IInspectable const&, PointerRoutedEventArgs const&) { OnReleased(); });
    surface_.KeyDown([this](IInspectable const&, KeyRoutedEventArgs const& args) {
        if (args.Key() == VirtualKey::Delete) {
            DeleteSelected();
            args.Handled(true);
        }
    });
    saveButton_.Click([this](auto&&, auto&&) { if (onSave_) onSave_(); });
    cancelButton_.Click([this](auto&&, auto&&) { if (onCancel_) onCancel_(); });
    deleteButton_.Click([this](auto&&, auto&&) { if (onDelete_) onDelete_(); });
}

void CanvasEditor::Load(hstring const& name, std::vector<layrix::NormRect> const& zones, bool canDelete) {
    deleteButton_.Visibility(canDelete ? Visibility::Visible : Visibility::Collapsed);
    nameBox_.Text(name);
    zones_ = zones;
    selected_ = zones_.empty() ? -1 : 0;
    mode_ = DragMode::None;
    Rebuild();
    surface_.Focus(FocusState::Programmatic);
}

std::vector<layrix::NormRect> CanvasEditor::Zones() const {
    return zones_;
}

hstring CanvasEditor::Name() {
    return nameBox_.Text();
}

void CanvasEditor::SetHandlers(std::function<void()> onSave, std::function<void()> onCancel, std::function<void()> onDelete) {
    onSave_ = std::move(onSave);
    onCancel_ = std::move(onCancel);
    onDelete_ = std::move(onDelete);
}

bool CanvasEditor::TryLocal(PointerRoutedEventArgs const& args, double& x, double& y) {
    const double width = surface_.ActualWidth();
    const double height = surface_.ActualHeight();
    if (width < 2.0 || height < 2.0) {
        return false;
    }
    const auto point = args.GetCurrentPoint(surface_).Position();
    x = std::clamp(point.X / width, 0.0, 1.0);
    y = std::clamp(point.Y / height, 0.0, 1.0);
    return true;
}

int CanvasEditor::HitTest(double x, double y, int& edges) {
    const double width = std::max(1.0, surface_.ActualWidth());
    const double height = std::max(1.0, surface_.ActualHeight());
    const double edgeX = 8.0 / width;
    const double edgeY = 8.0 / height;
    for (int index = static_cast<int>(zones_.size()) - 1; index >= 0; --index) {
        const auto& zone = zones_[static_cast<size_t>(index)];
        if (x < zone.x || y < zone.y || x > zone.x + zone.w || y > zone.y + zone.h) {
            continue;
        }
        edges = 0;
        if (x - zone.x <= edgeX) edges |= 1;
        if (zone.x + zone.w - x <= edgeX) edges |= 2;
        if (y - zone.y <= edgeY) edges |= 4;
        if (zone.y + zone.h - y <= edgeY) edges |= 8;
        if ((edges & 1) && (edges & 2)) edges = x < zone.x + zone.w * 0.5 ? 1 : 2;
        if ((edges & 4) && (edges & 8)) edges = (edges & ~12) | (y < zone.y + zone.h * 0.5 ? 4 : 8);
        return index;
    }
    edges = 0;
    return -1;
}

void CanvasEditor::OnPressed(PointerRoutedEventArgs const& args) {
    if (!args.GetCurrentPoint(surface_).Properties().IsLeftButtonPressed()) return;
    double x = 0;
    double y = 0;
    if (!TryLocal(args, x, y)) {
        return;
    }
    surface_.CapturePointer(args.Pointer());
    surface_.Focus(FocusState::Programmatic);
    int edges = 0;
    const int index = HitTest(x, y, edges);
    startX_ = x;
    startY_ = y;
    if (index < 0) {
        mode_ = DragMode::Create;
        dragIndex_ = -1;
        dragEdges_ = 0;
        return;
    }
    selected_ = index;
    dragIndex_ = index;
    original_ = zones_[static_cast<size_t>(index)];
    if (edges != 0) {
        mode_ = DragMode::Resize;
        dragEdges_ = edges;
    } else {
        mode_ = DragMode::Move;
        dragEdges_ = 0;
    }
    Rebuild();
}

void CanvasEditor::OnMoved(PointerRoutedEventArgs const& args) {
    if (mode_ == DragMode::None) {
        return;
    }
    double x = 0;
    double y = 0;
    if (!TryLocal(args, x, y)) {
        return;
    }
    if (mode_ == DragMode::Create) {
        layrix::NormRect zone;
        zone.x = std::min(startX_, x);
        zone.y = std::min(startY_, y);
        zone.w = std::max(0.01, std::abs(x - startX_));
        zone.h = std::max(0.01, std::abs(y - startY_));
        if (dragIndex_ < 0) {
            zones_.push_back(zone);
            dragIndex_ = static_cast<int>(zones_.size()) - 1;
        } else {
            zones_[static_cast<size_t>(dragIndex_)] = zone;
        }
        selected_ = dragIndex_;
        Rebuild();
        return;
    }

    layrix::NormRect zone = original_;
    if (mode_ == DragMode::Move) {
        zone.x += x - startX_;
        zone.y += y - startY_;
        if (zone.x < 0) zone.x = 0;
        if (zone.y < 0) zone.y = 0;
        if (zone.x + zone.w > 1.0) zone.x = 1.0 - zone.w;
        if (zone.y + zone.h > 1.0) zone.y = 1.0 - zone.h;
        SnapMove(zone, dragIndex_);
        layrix::ClampZone(zone);
    } else {
        if (dragEdges_ & 1) {
            const double right = original_.x + original_.w;
            zone.x = std::min(x, right - 0.05);
            zone.w = right - zone.x;
        }
        if (dragEdges_ & 2) {
            zone.w = std::max(0.05, x - zone.x);
        }
        if (dragEdges_ & 4) {
            const double bottom = original_.y + original_.h;
            zone.y = std::min(y, bottom - 0.05);
            zone.h = bottom - zone.y;
        }
        if (dragEdges_ & 8) {
            zone.h = std::max(0.05, y - zone.y);
        }
        SnapResize(zone, dragEdges_, dragIndex_);
        layrix::ClampZone(zone);
    }
    if (dragIndex_ >= 0 && dragIndex_ < static_cast<int>(zones_.size())) {
        zones_[static_cast<size_t>(dragIndex_)] = zone;
        selected_ = dragIndex_;
        Rebuild();
    }
}

void CanvasEditor::OnReleased() {
    if (mode_ == DragMode::None) {
        return;
    }
    if (dragIndex_ >= 0 && dragIndex_ < static_cast<int>(zones_.size())) {
        auto& zone = zones_[static_cast<size_t>(dragIndex_)];
        if (zone.w < 0.04 || zone.h < 0.04) {
            zones_.erase(zones_.begin() + dragIndex_);
            selected_ = -1;
        } else {
            layrix::ClampZone(zone);
            selected_ = dragIndex_;
        }
    }
    mode_ = DragMode::None;
    dragIndex_ = -1;
    surface_.ReleasePointerCaptures();
    Rebuild();
}

void CanvasEditor::DeleteSelected() {
    if (selected_ < 0 || selected_ >= static_cast<int>(zones_.size())) {
        return;
    }
    zones_.erase(zones_.begin() + selected_);
    selected_ = zones_.empty() ? -1 : std::min(selected_, static_cast<int>(zones_.size()) - 1);
    Rebuild();
}

void CanvasEditor::SnapMove(layrix::NormRect& zone, int index) const {
    std::vector<double> xs{0.0, 1.0};
    std::vector<double> ys{0.0, 1.0};
    for (int other = 0; other < static_cast<int>(zones_.size()); ++other) {
        if (other == index) continue;
        xs.push_back(zones_[static_cast<size_t>(other)].x);
        xs.push_back(zones_[static_cast<size_t>(other)].x + zones_[static_cast<size_t>(other)].w);
        ys.push_back(zones_[static_cast<size_t>(other)].y);
        ys.push_back(zones_[static_cast<size_t>(other)].y + zones_[static_cast<size_t>(other)].h);
    }
    const double left = SnapTo(zone.x, xs);
    const double right = SnapTo(zone.x + zone.w, xs);
    if (std::abs(left - zone.x) <= std::abs(right - (zone.x + zone.w))) {
        zone.x = left;
    } else {
        zone.x = right - zone.w;
    }
    const double top = SnapTo(zone.y, ys);
    const double bottom = SnapTo(zone.y + zone.h, ys);
    if (std::abs(top - zone.y) <= std::abs(bottom - (zone.y + zone.h))) {
        zone.y = top;
    } else {
        zone.y = bottom - zone.h;
    }
}

void CanvasEditor::SnapResize(layrix::NormRect& zone, int edges, int index) const {
    std::vector<double> xs{0.0, 1.0};
    std::vector<double> ys{0.0, 1.0};
    for (int other = 0; other < static_cast<int>(zones_.size()); ++other) {
        if (other == index) continue;
        xs.push_back(zones_[other].x);
        xs.push_back(zones_[other].x + zones_[other].w);
        ys.push_back(zones_[other].y);
        ys.push_back(zones_[other].y + zones_[other].h);
    }
    if (edges & 1) {
        const double right = zone.x + zone.w;
        zone.x = std::min(SnapTo(zone.x, xs), right - 0.05);
        zone.w = right - zone.x;
    }
    if (edges & 2) {
        const double right = SnapTo(zone.x + zone.w, xs);
        zone.w = std::max(0.05, right - zone.x);
    }
    if (edges & 4) {
        const double bottom = zone.y + zone.h;
        zone.y = std::min(SnapTo(zone.y, ys), bottom - 0.05);
        zone.h = bottom - zone.y;
    }
    if (edges & 8) {
        const double bottom = SnapTo(zone.y + zone.h, ys);
        zone.h = std::max(0.05, bottom - zone.y);
    }
}

void CanvasEditor::Rebuild() {
    saveButton_.IsEnabled(!zones_.empty());
    zoneCanvas_.Children().Clear();
    const double width = std::max(1.0, surface_.ActualWidth());
    const double height = std::max(1.0, surface_.ActualHeight());
    for (int index = 0; index < static_cast<int>(zones_.size()); ++index) {
        const auto& zone = zones_[static_cast<size_t>(index)];
        Border box;
        box.IsHitTestVisible(false);
        box.Width(std::max(2.0, zone.w * width));
        box.Height(std::max(2.0, zone.h * height));
        box.CornerRadius({8, 8, 8, 8});
        box.Background(Paint(40, 40, 40, 160));
        const bool selected = index == selected_;
        box.BorderBrush(selected ? Paint(230, 230, 230, 180) : Paint(255, 255, 255, 48));
        const double border = selected ? 2.0 : 1.0;
        box.BorderThickness(Thickness{border, border, border, border});
        TextBlock label;
        label.Text(to_hstring(index + 1));
        label.Margin(Thickness{8, 6, 0, 0});
        label.FontSize(13);
        label.Foreground(Paint(220, 220, 220));
        box.Child(label);
        Canvas::SetLeft(box, zone.x * width);
        Canvas::SetTop(box, zone.y * height);
        zoneCanvas_.Children().Append(box);
    }
}

}  // namespace winrt::LayrixEditor::implementation
