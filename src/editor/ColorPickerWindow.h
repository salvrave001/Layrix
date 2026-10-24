#pragma once

#include <windows.h>

#include <functional>
#include <string>

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>

namespace layrix {

class ColorPickerWindow {
public:
    ColorPickerWindow();
    ~ColorPickerWindow();

    ColorPickerWindow(const ColorPickerWindow&) = delete;
    ColorPickerWindow& operator=(const ColorPickerWindow&) = delete;

    void Open(HWND owner, POINT anchor, COLORREF initial, std::function<void(COLORREF)> onPreview, std::function<void(COLORREF)> onCommit);
    void Close();
    bool IsOpen() const;

private:
    enum class Tab { Rgb, Hsv, Hex };

    void BuildUi();
    void StyleWindow();
    void PlaceAboveOwner(HWND owner, POINT anchor);
    HWND NativeHandle() const;
    void SetColor(COLORREF color, bool commit);
    void SetHsv(float hue, float sat, float val, bool commit);
    void RebuildWheel();
    void RebuildSlider();
    void UpdateCursor();
    void UpdateSliderThumb();
    void SyncFields();
    void ShowTab(Tab tab);
    void OnWheelPointer(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
    void OnSliderPointer(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
    void CommitIfNeeded();
    winrt::Microsoft::UI::Xaml::Controls::TextBox MakeField();
    winrt::Microsoft::UI::Xaml::Controls::Grid MakeFieldRow(wchar_t const* label, winrt::Microsoft::UI::Xaml::Controls::TextBox const& box);
    void BindField(winrt::Microsoft::UI::Xaml::Controls::TextBox const& box, std::function<void(std::wstring const&)> apply);
    static void RgbToHsv(float r, float g, float b, float& h, float& s, float& v);
    static void HsvToRgb(float h, float s, float v, float& r, float& g, float& b);
    COLORREF CurrentColor() const;

    winrt::Microsoft::UI::Xaml::Window window_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Image wheelImage_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Canvas wheelCanvas_{nullptr};
    winrt::Microsoft::UI::Xaml::Shapes::Ellipse wheelDot_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Image sliderImage_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Canvas sliderCanvas_{nullptr};
    winrt::Microsoft::UI::Xaml::Shapes::Ellipse sliderThumb_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Button rgbTab_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Button hsvTab_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Button hexTab_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::StackPanel fields_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBox fieldA_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBox fieldB_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBox fieldC_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBox fieldD_{nullptr};

    std::function<void(COLORREF)> onPreview_;
    std::function<void(COLORREF)> onCommit_;
    float hue_ = 0;
    float sat_ = 0;
    float val_ = 1;
    float alpha_ = 1;
    Tab tab_ = Tab::Hsv;
    bool suppress_ = false;
    bool dragging_ = false;
    COLORREF lastCommit_ = 0;
};

}  // namespace layrix
