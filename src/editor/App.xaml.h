#pragma once

#include "App.g.h"

namespace winrt::LayrixEditor::implementation {

struct App : AppT<App> {
    App();
    void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);
};

}  // namespace winrt::LayrixEditor::implementation

namespace winrt::LayrixEditor::factory_implementation {

struct App : AppT<App, implementation::App> {};

}  // namespace winrt::LayrixEditor::factory_implementation
