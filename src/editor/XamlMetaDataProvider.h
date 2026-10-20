#pragma once

#include "XamlMetaDataProvider.g.h"
#include "XamlTypeInfo.xaml.g.h"

namespace winrt::LayrixEditor::implementation {
struct XamlMetaDataProvider : XamlMetaDataProviderT<XamlMetaDataProvider> {
    XamlMetaDataProvider() = default;
    Microsoft::UI::Xaml::Markup::IXamlType GetXamlType(Windows::UI::Xaml::Interop::TypeName const& type);
    Microsoft::UI::Xaml::Markup::IXamlType GetXamlType(hstring const& fullName);
    com_array<Microsoft::UI::Xaml::Markup::XmlnsDefinition> GetXmlnsDefinitions();
private:
    std::shared_ptr<XamlTypeInfoProvider> Provider();
    std::shared_ptr<XamlTypeInfoProvider> _provider;
};
}

namespace winrt::LayrixEditor::factory_implementation {
struct XamlMetaDataProvider : XamlMetaDataProviderT<XamlMetaDataProvider, implementation::XamlMetaDataProvider> {};
}
