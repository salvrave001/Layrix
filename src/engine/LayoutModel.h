#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace layrix {

enum class LayoutType {
    None,
    Focus,
    Columns,
    Rows,
    Grid,
    PriorityGrid,
    Canvas
};

struct NormRect {
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;
};

struct Layout {
    std::wstring id;
    std::wstring name;
    LayoutType type = LayoutType::Columns;
    int columns = 3;
    int rows = 2;
    int spacing = 8;
    std::vector<NormRect> zones;
    bool builtin = false;
};

struct AppSettings {
    std::wstring highlightColor = L"#4CC2FF";
    bool overrideWinArrows = true;
    bool zonesEnabled = true;
    std::vector<Layout> layouts;
    std::vector<std::pair<std::wstring, std::wstring>> monitorLayouts;

    const Layout* FindLayout(std::wstring_view id) const;
    Layout* FindLayout(std::wstring_view id);
    std::wstring LayoutForMonitor(std::wstring_view key, std::wstring_view device) const;
    void SetMonitorLayout(std::wstring key, std::wstring layoutId);
};

std::wstring_view LayoutTypeToString(LayoutType type);
std::optional<LayoutType> LayoutTypeFromString(std::wstring_view text);

std::vector<NormRect> BuildNormalizedZones(const Layout& layout);
RECT ZoneToPixels(const NormRect& zone, const RECT& work, int spacing);
RECT UnionRects(const std::vector<RECT>& rects);

bool ParseHexColor(std::wstring_view text, COLORREF& color);
std::wstring ColorToHex(COLORREF color);
void ClampZone(NormRect& zone);

std::wstring NextCustomId(const AppSettings& settings);
std::wstring NextCustomName(const AppSettings& settings);

}  // namespace layrix
