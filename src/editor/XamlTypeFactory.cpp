#include "pch.h"

#include "XamlTypeInfo.xaml.g.h"
#include "CanvasEditor.xaml.h"
#include "EditorWindow.xaml.h"
#include "TemplateCard.xaml.h"

namespace winrt::LayrixEditor::implementation {
namespace {

using IInspectable = winrt::Windows::Foundation::IInspectable;

IInspectable ActivateEditorWindow() {
    return winrt::make<EditorWindow>();
}

IInspectable ActivateTemplateCard() {
    return winrt::make<TemplateCard>();
}

IInspectable ActivateCanvasEditor() {
    return winrt::make<CanvasEditor>();
}

IXamlType MakeLocalType(std::shared_ptr<XamlTypeInfoProvider> const& provider, wchar_t const* name, wchar_t const* baseName, XamlUserType::ActivatorFn activator) {
    auto base = winrt::make<XamlSystemBaseType>(winrt::hstring{baseName});
    auto user = winrt::make_self<XamlUserType>(provider, winrt::hstring{name}, base);
    user->IsLocalType(true);
    user->_kindOfType = winrt::Windows::UI::Xaml::Interop::TypeKind::Metadata;
    user->_activator = activator;
    return *user;
}

}  // namespace

IXamlType XamlTypeInfoProvider::CreateXamlType(winrt::hstring const& typeName) {
    auto provider = shared_from_this();
    if (typeName == L"LayrixEditor.EditorWindow") {
        return MakeLocalType(provider, L"LayrixEditor.EditorWindow", L"Microsoft.UI.Xaml.Window", ActivateEditorWindow);
    }
    if (typeName == L"LayrixEditor.TemplateCard") {
        return MakeLocalType(provider, L"LayrixEditor.TemplateCard", L"Microsoft.UI.Xaml.Controls.UserControl", ActivateTemplateCard);
    }
    if (typeName == L"LayrixEditor.CanvasEditor") {
        return MakeLocalType(provider, L"LayrixEditor.CanvasEditor", L"Microsoft.UI.Xaml.Controls.UserControl", ActivateCanvasEditor);
    }
    if (typeName == L"LayrixEditor.App") {
        auto base = winrt::make<XamlSystemBaseType>(winrt::hstring{L"Microsoft.UI.Xaml.Application"});
        auto user = winrt::make_self<XamlUserType>(provider, winrt::hstring{L"LayrixEditor.App"}, base);
        user->IsLocalType(true);
        user->_kindOfType = winrt::Windows::UI::Xaml::Interop::TypeKind::Metadata;
        return *user;
    }
    return nullptr;
}

IXamlMember XamlTypeInfoProvider::CreateXamlMember(winrt::hstring const&) {
    return nullptr;
}

std::vector<winrt::Microsoft::UI::Xaml::Markup::IXamlMetadataProvider> const& XamlTypeInfoProvider::OtherProviders() {
    if (_otherProviders.empty()) {
        _otherProviders.push_back(winrt::Microsoft::UI::Xaml::XamlTypeInfo::XamlControlsXamlMetaDataProvider());
    }
    return _otherProviders;
}

}  // namespace winrt::LayrixEditor::implementation
