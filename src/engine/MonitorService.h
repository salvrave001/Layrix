#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace layrix {

struct MonitorInfo {
    int index = 0;
    HMONITOR handle = nullptr;
    std::wstring device;
    std::wstring key;
    RECT bounds{};
    RECT work{};
};

std::vector<MonitorInfo> EnumerateMonitors();

}  // namespace layrix
