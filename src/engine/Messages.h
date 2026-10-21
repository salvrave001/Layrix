#pragma once

#include <windows.h>

namespace layrix {

constexpr wchar_t kEngineClass[] = L"LayrixEngine";
constexpr wchar_t kOverlayClass[] = L"LayrixOverlay";
constexpr wchar_t kBadgeClass[] = L"LayrixBadge";

constexpr UINT kMsgReload = WM_APP + 1;
constexpr UINT kMsgOpenEditor = WM_APP + 2;
constexpr UINT kMsgTray = WM_APP + 3;
constexpr UINT kMsgFlash = WM_APP + 4;
constexpr UINT kMsgHover = WM_APP + 5;
constexpr UINT kMsgDragEnd = WM_APP + 6;
constexpr UINT kMsgHotkey = WM_APP + 7;

constexpr UINT_PTR kTimerSnap = 1;
constexpr UINT_PTR kTimerBadge = 2;
constexpr UINT_PTR kTimerDrag = 3;
constexpr UINT_PTR kTimerTray = 4;

}  // namespace layrix
