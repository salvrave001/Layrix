#pragma once

#include "TemplateCard.g.h"
#include "engine/LayoutModel.h"

#include <functional>
#include <vector>

namespace winrt::LayrixEditor::implementation {

struct TemplateCard : TemplateCardT<TemplateCard> {
    TemplateCard();
    void SetTitle(winrt::hstring const& title);
    void SetCardWidth(double width);
    void SetZones(std::vector<layrix::NormRect> const& zones);
    void SetSelected(bool selected);
    void SetAccent(winrt::Windows::UI::Color color);
    void SetOnClick(std::function<void()> callback);
    void SetOnEdit(std::function<void()> callback);
    void SetCustomMenu(std::function<void()> onRename, std::function<void()> onDelete);

private:
    void Redraw();
    bool IsEditSource(winrt::Windows::Foundation::IInspectable const& source);
    void BuildCustomMenu();

    std::vector<layrix::NormRect> zones_;
    winrt::Windows::UI::Color accent_{255, 76, 194, 255};
    bool selected_ = false;
    std::function<void()> onClick_;
    std::function<void()> onEdit_;
    std::function<void()> onRename_;
    std::function<void()> onDelete_;
};

}  // namespace winrt::LayrixEditor::implementation

namespace winrt::LayrixEditor::factory_implementation {

struct TemplateCard : TemplateCardT<TemplateCard, implementation::TemplateCard> {};

}  // namespace winrt::LayrixEditor::factory_implementation
