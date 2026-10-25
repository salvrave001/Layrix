#include "engine/SnapEngine.h"

#include "engine/Messages.h"
#include "engine/UiStrings.h"
#include "engine/WindowFilter.h"
#include "overlay/ZoneOverlay.h"

#include <dwmapi.h>

#include <algorithm>
#include <optional>

namespace layrix {
namespace {
SnapEngine* g_engine = nullptr;

long DistanceSquared(POINT point, const RECT& rect) {
    const long x = std::clamp(point.x, rect.left, rect.right);
    const long y = std::clamp(point.y, rect.top, rect.bottom);
    const long dx = point.x - x;
    const long dy = point.y - y;
    return dx * dx + dy * dy;
}
}

SnapEngine::SnapEngine(HWND host)
    : host_(host),
      store_(LayoutStore::BesideExecutable()),
      hotkeys_(host) {
    g_engine = this;
    winevent_ = SetWinEventHook(
        EVENT_SYSTEM_MOVESIZESTART,
        EVENT_SYSTEM_MOVESIZEEND,
        nullptr,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    Reload();
}

SnapEngine::~SnapEngine() {
    KillTimer(host_, kTimerSnap);
    KillTimer(host_, kTimerBadge);
    KillTimer(host_, kTimerDrag);
    RemoveMouse();
    if (winevent_) {
        UnhookWinEvent(winevent_);
        winevent_ = nullptr;
    }
    HideOverlays();
    if (g_engine == this) {
        g_engine = nullptr;
    }
}

void SnapEngine::Reload() {
    settings_ = store_.Load();
    SetCurrentUiLanguage(ParseUiLanguage(settings_.language));
    hotkeys_.SetEnabled(settings_.zonesEnabled && settings_.overrideWinArrows);
    RebuildZones();
    if (!settings_.zonesEnabled && dragging_) {
        EndDrag();
        return;
    }
    RefreshOverlays();
}

COLORREF SnapEngine::ZoneColor() const {
    return ResolveColor(settings_.highlightColor);
}

void SnapEngine::RefreshOverlays() {
    if (dragging_) {
        OnDragTick();
        return;
    }
    HideOverlays();
}

void SnapEngine::SetZonesEnabled(bool enabled) {
    store_.Update([enabled](AppSettings& settings) { settings.zonesEnabled = enabled; });
    Reload();
}

void SnapEngine::RebuildZones() {
    monitors_ = EnumerateMonitors();
    zones_.assign(monitors_.size(), {});
    spacing_.assign(monitors_.size(), 0);
    overlays_.resize(monitors_.size());
    for (size_t index = 0; index < monitors_.size(); ++index) {
        const std::wstring layoutId = settings_.LayoutForMonitor(monitors_[index].key, monitors_[index].device);
        const Layout* layout = settings_.FindLayout(layoutId);
        if (!layout) {
            continue;
        }
        spacing_[index] = layout->spacing;
        for (const NormRect& zone : BuildNormalizedZones(*layout)) {
            zones_[index].push_back(ZoneToPixels(zone, monitors_[index].work, layout->spacing));
        }
        if (!overlays_[index]) {
            overlays_[index] = std::make_unique<ZoneOverlay>();
        }
    }
    if (!badge_) {
        badge_ = std::make_unique<MonitorBadge>();
    }
}

void SnapEngine::FlashMonitor(int index) {
    if (index < 0 || index >= static_cast<int>(monitors_.size())) {
        RebuildZones();
    }
    if (index < 0 || index >= static_cast<int>(monitors_.size()) || !badge_) {
        return;
    }
    badge_->Show(monitors_[static_cast<size_t>(index)].bounds, index + 1, ZoneColor());
    SetTimer(host_, kTimerBadge, 900, nullptr);
}

void SnapEngine::HideBadge() {
    KillTimer(host_, kTimerBadge);
    if (badge_) {
        badge_->Hide();
    }
}

void SnapEngine::MoveForegroundBy(int direction) {
    if (!settings_.zonesEnabled || direction == 0) {
        return;
    }
    HWND hwnd = GetForegroundWindow();
    if (!IsSnapTarget(hwnd, true)) {
        return;
    }
    const int monitor = MonitorIndex(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
    if (monitor < 0 || zones_[static_cast<size_t>(monitor)].empty()) {
        return;
    }
    const auto& monitorZones = zones_[static_cast<size_t>(monitor)];
    RECT windowRect{};
    GetWindowRect(hwnd, &windowRect);
    const POINT center{
        (windowRect.left + windowRect.right) / 2,
        (windowRect.top + windowRect.bottom) / 2,
    };
    int current = -1;
    for (int index = static_cast<int>(monitorZones.size()) - 1; index >= 0; --index) {
        if (PtInRect(&monitorZones[static_cast<size_t>(index)], center)) {
            current = index;
            break;
        }
    }
    int next = 0;
    if (current < 0) {
        next = direction > 0 ? 0 : static_cast<int>(monitorZones.size()) - 1;
    } else {
        const int count = static_cast<int>(monitorZones.size());
        next = (current + direction + count) % count;
    }
    SnapWindow(hwnd, monitorZones[static_cast<size_t>(next)]);
}

int SnapEngine::MonitorIndex(HMONITOR monitor) const {
    for (const MonitorInfo& info : monitors_) {
        if (info.handle == monitor) {
            return info.index;
        }
    }
    return -1;
}

void SnapEngine::SnapWindow(HWND hwnd, const RECT& rect) const {
    if (!IsWindow(hwnd)) {
        return;
    }
    if (IsZoomed(hwnd) || IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    // Zones describe visible edges. GetWindowRect also includes the invisible
    // resize borders, which otherwise leave a gap even when spacing is zero.
    RECT target = rect;
    RECT windowRect{}, frameRect{};
    if (GetWindowRect(hwnd, &windowRect) &&
        SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
            &frameRect, sizeof(frameRect))) &&
        frameRect.right > frameRect.left && frameRect.bottom > frameRect.top) {
        target.left -= std::max(0L, frameRect.left - windowRect.left);
        target.top -= std::max(0L, frameRect.top - windowRect.top);
        target.right += std::max(0L, windowRect.right - frameRect.right);
        target.bottom += std::max(0L, windowRect.bottom - frameRect.bottom);
    }
    SetWindowPos(
        hwnd,
        nullptr,
        target.left,
        target.top,
        std::max(1L, target.right - target.left),
        std::max(1L, target.bottom - target.top),
        SWP_NOZORDER | SWP_NOACTIVATE);
}

void CALLBACK SnapEngine::WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG, LONG, DWORD, DWORD) {
    if (!g_engine || !hwnd) {
        return;
    }
    if (event == EVENT_SYSTEM_MOVESIZESTART) {
        g_engine->BeginDrag(hwnd);
    } else if (event == EVENT_SYSTEM_MOVESIZEEND) {
        PostMessageW(g_engine->host_, kMsgDragEnd, 0, 0);
    }
}

void SnapEngine::BeginDrag(HWND target) {
    if (!settings_.zonesEnabled || dragging_ || !IsSnapTarget(target)) {
        return;
    }
    dragging_ = true;
    dragTarget_ = target;
    selection_.clear();
    hotkeys_.SetTrackingCancel(true);
    RebuildZones();
    InstallMouse();
    // Poll modifiers even when the pointer is stationary during a drag.
    SetTimer(host_, kTimerDrag, 16, nullptr);
    OnDragTick();
}

void SnapEngine::InstallMouse() {
    if (!mouse_) {
        mouse_ = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(nullptr), 0);
    }
}

void SnapEngine::RemoveMouse() {
    if (mouse_) {
        UnhookWindowsHookEx(mouse_);
        mouse_ = nullptr;
    }
    hoverQueued_ = false;
}

LRESULT CALLBACK SnapEngine::MouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_engine && (wParam == WM_MOUSEMOVE || wParam == WM_LBUTTONUP)) {
        const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        g_engine->hoverPoint_ = info->pt;
        if (!g_engine->hoverQueued_) {
            g_engine->hoverQueued_ = true;
            PostMessageW(g_engine->host_, kMsgHover, 0, 0);
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void SnapEngine::OnHover() {
    hoverQueued_ = false;
    if (dragging_) {
        UpdateHover(hoverPoint_);
    }
}

void SnapEngine::OnDragTick() {
    POINT point{};
    if (dragging_ && GetCursorPos(&point)) {
        UpdateHover(point);
    }
}

void SnapEngine::UpdateHover(POINT point) {
    if (!KeyChordHeld(settings_.snapHold)) {
        selection_.clear();
        if (overlaysActive_) {
            HideOverlays();
        }
        return;
    }
    const bool extend = KeyChordExtendHeld(settings_.snapHold);
    std::optional<ZoneGeom> hit;
    for (int monitor = 0; monitor < static_cast<int>(monitors_.size()); ++monitor) {
        if (!PtInRect(&monitors_[static_cast<size_t>(monitor)].bounds, point)) {
            continue;
        }
        const auto& monitorZones = zones_[static_cast<size_t>(monitor)];
        const int slop = spacing_[static_cast<size_t>(monitor)] / 2;
        for (int index = static_cast<int>(monitorZones.size()) - 1; index >= 0; --index) {
            if (PtInRect(&monitorZones[static_cast<size_t>(index)], point)) {
                hit = ZoneGeom{monitor, index, monitorZones[static_cast<size_t>(index)]};
                break;
            }
        }
        if (!hit) {
            long best = MAXLONG;
            for (int index = 0; index < static_cast<int>(monitorZones.size()); ++index) {
                RECT inflated = monitorZones[static_cast<size_t>(index)];
                InflateRect(&inflated, slop, slop);
                if (!PtInRect(&inflated, point)) {
                    continue;
                }
                const long distance = DistanceSquared(point, monitorZones[static_cast<size_t>(index)]);
                if (distance < best) {
                    best = distance;
                    hit = ZoneGeom{monitor, index, monitorZones[static_cast<size_t>(index)]};
                }
            }
        }
        break;
    }

    std::vector<ZoneGeom> next;
    if (!extend) {
        if (hit) {
            next.push_back(*hit);
        }
    } else if (hit) {
        next = selection_;
        const bool sameMonitor = next.empty() || next.front().monitor == hit->monitor;
        const bool already = std::any_of(next.begin(), next.end(), [&](const ZoneGeom& zone) {
            return zone.monitor == hit->monitor && zone.index == hit->index;
        });
        if (sameMonitor && !already) {
            next.push_back(*hit);
        }
    } else {
        next = selection_;
    }

    const bool changed = next.size() != selection_.size() ||
        !std::equal(next.begin(), next.end(), selection_.begin(), [](const ZoneGeom& left, const ZoneGeom& right) {
            return left.monitor == right.monitor && left.index == right.index;
        });
    if (changed) {
        selection_ = std::move(next);
    }
    if (settings_.zoneDisplay == ZoneDisplayMode::Hidden) {
        HideOverlays();
        return;
    }
    if (changed || !overlaysActive_) {
        Paint();
    }
}

void SnapEngine::Paint() {
    overlaysActive_ = true;
    const COLORREF color = ZoneColor();
    std::optional<RECT> highlight;
    int highlightMonitor = -1;
    if (!selection_.empty()) {
        std::vector<RECT> rects;
        rects.reserve(selection_.size());
        for (const ZoneGeom& zone : selection_) {
            rects.push_back(zone.rect);
        }
        highlight = UnionRects(rects);
        highlightMonitor = selection_.front().monitor;
    }
    for (size_t index = 0; index < overlays_.size(); ++index) {
        if (!overlays_[index]) {
            continue;
        }
        if (zones_[index].empty()) {
            overlays_[index]->Hide();
            continue;
        }
        const std::optional<RECT> local = static_cast<int>(index) == highlightMonitor ? highlight : std::nullopt;
        overlays_[index]->Show(
            monitors_[index].work,
            zones_[index],
            local,
            color,
            settings_.zoneDisplay == ZoneDisplayMode::OnTop);
    }
}

void SnapEngine::HideOverlays() {
    overlaysActive_ = false;
    for (const auto& overlay : overlays_) {
        if (overlay) {
            overlay->Hide();
        }
    }
}

void SnapEngine::EndDrag() {
    if (!dragging_) {
        return;
    }
    // Refresh the final hit and modifier state; the last mouse message may
    // still be queued when Windows reports the end of its move loop.
    OnDragTick();
    KillTimer(host_, kTimerDrag);
    const bool cancel = hotkeys_.ConsumeCancel();
    hotkeys_.SetTrackingCancel(false);
    const HWND target = dragTarget_;
    const std::vector<ZoneGeom> selected = selection_;
    dragging_ = false;
    dragTarget_ = nullptr;
    selection_.clear();
    RemoveMouse();
    RefreshOverlays();
    if (cancel || !settings_.zonesEnabled || !IsWindow(target) || selected.empty()) {
        return;
    }
    std::vector<RECT> rects;
    rects.reserve(selected.size());
    for (const ZoneGeom& zone : selected) {
        rects.push_back(zone.rect);
    }
    pendingHwnd_ = target;
    pendingRect_ = UnionRects(rects);
    SetTimer(host_, kTimerSnap, 40, nullptr);
}

void SnapEngine::FinishPendingSnap() {
    KillTimer(host_, kTimerSnap);
    const HWND hwnd = pendingHwnd_;
    const RECT rect = pendingRect_;
    pendingHwnd_ = nullptr;
    if (hwnd) {
        SnapWindow(hwnd, rect);
    }
}

}  // namespace layrix
