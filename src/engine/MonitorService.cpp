#include "engine/MonitorService.h"

#include <algorithm>

namespace layrix {
namespace {

BOOL CALLBACK CollectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM param) {
    auto* list = reinterpret_cast<std::vector<MonitorInfo>*>(param);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return TRUE;
    }

    MonitorInfo item;
    item.handle = monitor;
    item.device = info.szDevice;
    item.bounds = info.rcMonitor;
    item.work = info.rcWork;
    const int width = std::max(1L, item.work.right - item.work.left);
    const int height = std::max(1L, item.work.bottom - item.work.top);
    item.key = item.device + L"|" + std::to_wstring(width) + L"x" + std::to_wstring(height);
    list->push_back(std::move(item));
    return TRUE;
}

}  // namespace

std::vector<MonitorInfo> EnumerateMonitors() {
    std::vector<MonitorInfo> monitors;
    EnumDisplayMonitors(nullptr, nullptr, CollectMonitor, reinterpret_cast<LPARAM>(&monitors));
    std::sort(monitors.begin(), monitors.end(), [](const MonitorInfo& left, const MonitorInfo& right) {
        if (left.work.left != right.work.left) {
            return left.work.left < right.work.left;
        }
        return left.work.top < right.work.top;
    });
    for (int index = 0; index < static_cast<int>(monitors.size()); ++index) {
        monitors[static_cast<size_t>(index)].index = index;
    }
    return monitors;
}

}  // namespace layrix
