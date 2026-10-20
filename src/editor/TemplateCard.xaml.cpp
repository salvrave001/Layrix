#include "pch.h"
#include "TemplateCard.xaml.h"

#include <algorithm>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Input;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Shapes;
using namespace Windows::UI;

namespace {
SolidColorBrush Paint(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return SolidColorBrush(ColorHelper::FromArgb(a, r, g, b));
}
}

namespace winrt::LayrixEditor::implementation {

TemplateCard::TemplateCard() {
    InitializeComponent();
    Card().Tapped([this](auto&&, TappedRoutedEventArgs const& args) {
        if (!IsEditSource(args.OriginalSource())) {
            if (onClick_) onClick_();
        }
    });
    EditButton().Click([this](auto&&, RoutedEventArgs const&) {
        if (onEdit_) onEdit_();
    });
    PointerEntered([this](auto&&, auto&&) {
        if (!selected_) Card().Background(Paint(62, 62, 62));
    });
    PointerExited([this](auto&&, auto&&) {
        if (!selected_) Card().Background(Paint(51, 51, 51));
    });
}

void TemplateCard::SetTitle(hstring const& title) {
    TitleText().Text(title);
}

void TemplateCard::SetZones(std::vector<layrix::NormRect> const& zones) {
    zones_ = zones;
    Redraw();
}

void TemplateCard::SetSelected(bool selected) {
    selected_ = selected;
    Card().BorderBrush(selected ? Paint(76, 194, 255) : Paint(58, 58, 58));
    const double border = selected ? 2.0 : 1.0;
    Card().BorderThickness(Thickness{border, border, border, border});
    Card().Background(Paint(51, 51, 51));
    TitleText().Foreground(selected ? Paint(76, 194, 255) : Paint(245, 245, 245));
}

void TemplateCard::SetOnClick(std::function<void()> callback) {
    onClick_ = std::move(callback);
}

void TemplateCard::SetOnEdit(std::function<void()> callback) {
    onEdit_ = std::move(callback);
}

void TemplateCard::Redraw() {
    Preview().Children().Clear();
    constexpr double width = 148.0;
    constexpr double height = 90.0;
    for (const auto& zone : zones_) {
        Microsoft::UI::Xaml::Shapes::Rectangle shape;
        const double left = zone.x * width + 1.0;
        const double top = zone.y * height + 1.0;
        shape.Width(std::max(2.0, zone.w * width - 2.0));
        shape.Height(std::max(2.0, zone.h * height - 2.0));
        shape.RadiusX(3);
        shape.RadiusY(3);
        shape.Fill(Paint(30, 30, 30));
        shape.Stroke(Paint(65, 65, 65));
        shape.StrokeThickness(1);
        Canvas::SetLeft(shape, left);
        Canvas::SetTop(shape, top);
        Preview().Children().Append(shape);
    }
}

bool TemplateCard::IsEditSource(IInspectable const& source) {
    auto node = source.try_as<DependencyObject>();
    while (node) {
        if (node == EditButton()) {
            return true;
        }
        node = VisualTreeHelper::GetParent(node);
    }
    return false;
}

}  // namespace winrt::LayrixEditor::implementation
