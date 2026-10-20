#pragma once

#include "engine/HotkeyController.h"
#include "engine/LayoutStore.h"
#include "engine/MonitorService.h"

#include <windows.h>

#include <memory>
#include <vector>

namespace layrix {

class ZoneOverlay;
class MonitorBadge;

struct ZoneGeom {
    int monitor = 0;
    int index = 0;
    RECT rect{};
};

class SnapEngine {
public:
    explicit SnapEngine(HWND host);
    ~SnapEngine();

    void Reload();
    bool ZonesEnabled() const { return settings_.zonesEnabled; }
    void SetZonesEnabled(bool enabled);
    void FlashMonitor(int index);
    void MoveForegroundBy(int direction);
    void OnHover();
    void OnDragTick();
    void EndDrag();
    void FinishPendingSnap();
    void HideBadge();

private:
    void RebuildZones();
    void BeginDrag(HWND target);
    void UpdateHover(POINT point);
    void Paint();
    void HideOverlays();
    void InstallMouse();
    void RemoveMouse();
    void SnapWindow(HWND hwnd, const RECT& rect) const;
    int MonitorIndex(HMONITOR monitor) const;

    static void CALLBACK WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG objectId, LONG childId, DWORD eventThread, DWORD eventTime);
    static LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam);

    HWND host_ = nullptr;
    LayoutStore store_;
    AppSettings settings_;
    std::vector<MonitorInfo> monitors_;
    std::vector<std::vector<RECT>> zones_;
    std::vector<int> spacing_;
    HotkeyController hotkeys_;
    std::vector<std::unique_ptr<ZoneOverlay>> overlays_;
    std::unique_ptr<MonitorBadge> badge_;
    HWINEVENTHOOK winevent_ = nullptr;
    HHOOK mouse_ = nullptr;
    bool dragging_ = false;
    bool overlaysActive_ = false;
    HWND dragTarget_ = nullptr;
    std::vector<ZoneGeom> selection_;
    HWND pendingHwnd_ = nullptr;
    RECT pendingRect_{};
    bool hoverQueued_ = false;
    POINT hoverPoint_{};
};

}  // namespace layrix
