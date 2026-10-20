#pragma once

#include "TemplateCard.g.h"
#include "engine/LayoutModel.h"

#include <functional>
#include <vector>

namespace winrt::LayrixEditor::implementation {

struct TemplateCard : TemplateCardT<TemplateCard> {
    TemplateCard();
    void SetTitle(winrt::hstring const& title);
    void SetZones(std::vector<layrix::NormRect> const& zones);
    void SetSelected(bool selected);
    void SetOnClick(std::function<void()> callback);
    void SetOnEdit(std::function<void()> callback);

private:
    void Redraw();
    bool IsEditSource(winrt::Windows::Foundation::IInspectable const& source);

    std::vector<layrix::NormRect> zones_;
    bool selected_ = false;
    std::function<void()> onClick_;
    std::function<void()> onEdit_;
};

}  // namespace winrt::LayrixEditor::implementation

namespace winrt::LayrixEditor::factory_implementation {

struct TemplateCard : TemplateCardT<TemplateCard, implementation::TemplateCard> {};

}  // namespace winrt::LayrixEditor::factory_implementation
