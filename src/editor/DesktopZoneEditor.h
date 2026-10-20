#pragma once
#include "engine/LayoutModel.h"
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <functional>
#include <memory>

namespace layrix {
class DesktopZoneEditor {
public:
    DesktopZoneEditor();
    ~DesktopZoneEditor();
    bool Open(HWND owner, RECT work, std::wstring name, std::vector<NormRect> zones,
              int spacing, COLORREF accent, std::function<void(bool)> finish);
    void Close();
    const std::vector<NormRect>& Zones() const { return zones_; }
    std::wstring Name() const;
private:
    enum { Add = 101, SplitX, SplitY, Delete, Save, Cancel };
    enum class Mode { None, Create, Move, Resize };
    static LRESULT CALLBACK Proc(HWND, UINT, WPARAM, LPARAM);
    LRESULT Handle(HWND, UINT, WPARAM, LPARAM);
    void Render();
    void Command(int command);
    void Finish(bool save);
    int Hit(POINT point, int& edges) const;
    void PointerDown(POINT point);
    void PointerMove(POINT point);
    void PointerUp();
    void UpdateButtons();
    double X(POINT p) const;
    double Y(POINT p) const;
    struct ToolsUi;
    std::unique_ptr<ToolsUi> tools_;
    HWND overlay_ = nullptr, panel_ = nullptr;
    HDC screen_ = nullptr, memory_ = nullptr;
    HBITMAP bitmap_ = nullptr, previous_ = nullptr;
    RECT work_{};
    int width_ = 0, height_ = 0, spacing_ = 0, selected_ = -1, edges_ = 0;
    double startX_ = 0, startY_ = 0;
    Mode mode_ = Mode::None;
    COLORREF accent_ = RGB(76, 194, 255);
    std::wstring name_;
    std::vector<NormRect> zones_, original_;
    std::function<void(bool)> finish_;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> target_;
    Microsoft::WRL::ComPtr<IDWriteFactory> write_;
};
}
