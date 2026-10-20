#include "engine/LayoutModel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace layrix {
namespace {

int Scale(double value, int size) {
    return static_cast<int>(std::llround(static_cast<long double>(value) * static_cast<long double>(size)));
}

}  // namespace

const Layout* AppSettings::FindLayout(std::wstring_view id) const {
    for (const Layout& layout : layouts) {
        if (layout.id == id) {
            return &layout;
        }
    }
    return nullptr;
}

Layout* AppSettings::FindLayout(std::wstring_view id) {
    for (Layout& layout : layouts) {
        if (layout.id == id) {
            return &layout;
        }
    }
    return nullptr;
}

std::wstring AppSettings::LayoutForMonitor(std::wstring_view key, std::wstring_view device) const {
    for (const auto& [monitorKey, layoutId] : monitorLayouts) {
        if (monitorKey == key) {
            return layoutId;
        }
    }
    const std::wstring prefix = std::wstring(device) + L"|";
    for (const auto& [monitorKey, layoutId] : monitorLayouts) {
        if (monitorKey.starts_with(prefix)) {
            return layoutId;
        }
    }
    return L"columns";
}

void AppSettings::SetMonitorLayout(std::wstring key, std::wstring layoutId) {
    for (auto& entry : monitorLayouts) {
        if (entry.first == key) {
            entry.second = std::move(layoutId);
            return;
        }
    }
    monitorLayouts.emplace_back(std::move(key), std::move(layoutId));
}

std::wstring_view LayoutTypeToString(LayoutType type) {
    switch (type) {
    case LayoutType::None:
        return L"none";
    case LayoutType::Focus:
        return L"focus";
    case LayoutType::Columns:
        return L"columns";
    case LayoutType::Rows:
        return L"rows";
    case LayoutType::Grid:
        return L"grid";
    case LayoutType::PriorityGrid:
        return L"priority";
    case LayoutType::Canvas:
        return L"canvas";
    }
    return L"columns";
}

std::optional<LayoutType> LayoutTypeFromString(std::wstring_view text) {
    if (text == L"none") return LayoutType::None;
    if (text == L"focus") return LayoutType::Focus;
    if (text == L"columns") return LayoutType::Columns;
    if (text == L"rows") return LayoutType::Rows;
    if (text == L"grid") return LayoutType::Grid;
    if (text == L"priority") return LayoutType::PriorityGrid;
    if (text == L"canvas") return LayoutType::Canvas;
    return std::nullopt;
}

std::vector<NormRect> BuildNormalizedZones(const Layout& layout) {
    const int columns = std::clamp(layout.columns, 1, 24);
    const int rows = std::clamp(layout.rows, 1, 24);
    switch (layout.type) {
    case LayoutType::None:
        return {};
    case LayoutType::Focus:
        return {
            {0.0, 0.0, 1.0, 1.0},
            {0.30, 0.28, 0.40, 0.44},
        };
    case LayoutType::Columns: {
        std::vector<NormRect> zones;
        zones.reserve(static_cast<size_t>(columns));
        for (int column = 0; column < columns; ++column) {
            zones.push_back({static_cast<double>(column) / columns, 0.0, 1.0 / columns, 1.0});
        }
        return zones;
    }
    case LayoutType::Rows: {
        std::vector<NormRect> zones;
        zones.reserve(static_cast<size_t>(rows));
        for (int row = 0; row < rows; ++row) {
            zones.push_back({0.0, static_cast<double>(row) / rows, 1.0, 1.0 / rows});
        }
        return zones;
    }
    case LayoutType::Grid: {
        std::vector<NormRect> zones;
        zones.reserve(static_cast<size_t>(columns * rows));
        for (int row = 0; row < rows; ++row) {
            for (int column = 0; column < columns; ++column) {
                zones.push_back({
                    static_cast<double>(column) / columns,
                    static_cast<double>(row) / rows,
                    1.0 / columns,
                    1.0 / rows,
                });
            }
        }
        return zones;
    }
    case LayoutType::PriorityGrid:
        return {
            {0.0, 0.0, 0.5, 1.0},
            {0.5, 0.0, 0.5, 0.5},
            {0.5, 0.5, 0.5, 0.5},
        };
    case LayoutType::Canvas:
        return layout.zones;
    }
    return {};
}

RECT ZoneToPixels(const NormRect& zone, const RECT& work, int spacing) {
    const int width = std::max(1L, work.right - work.left);
    const int height = std::max(1L, work.bottom - work.top);
    RECT rect;
    rect.left = work.left + Scale(zone.x, width);
    rect.top = work.top + Scale(zone.y, height);
    rect.right = work.left + Scale(zone.x + zone.w, width);
    rect.bottom = work.top + Scale(zone.y + zone.h, height);

    const int inset = std::max(0, spacing) / 2;
    rect.left += inset;
    rect.top += inset;
    rect.right -= inset;
    rect.bottom -= inset;
    if (rect.right <= rect.left) {
        rect.right = rect.left + 1;
    }
    if (rect.bottom <= rect.top) {
        rect.bottom = rect.top + 1;
    }
    return rect;
}

RECT UnionRects(const std::vector<RECT>& rects) {
    RECT result{0, 0, 0, 0};
    if (rects.empty()) {
        return result;
    }
    result = rects.front();
    for (size_t index = 1; index < rects.size(); ++index) {
        result.left = std::min(result.left, rects[index].left);
        result.top = std::min(result.top, rects[index].top);
        result.right = std::max(result.right, rects[index].right);
        result.bottom = std::max(result.bottom, rects[index].bottom);
    }
    return result;
}

bool ParseHexColor(std::wstring_view text, COLORREF& color) {
    if (!text.empty() && text.front() == L'#') {
        text.remove_prefix(1);
    }
    if (text.size() != 6) {
        return false;
    }
    unsigned value = 0;
    for (wchar_t ch : text) {
        value <<= 4;
        if (ch >= L'0' && ch <= L'9') {
            value |= static_cast<unsigned>(ch - L'0');
        } else if (ch >= L'a' && ch <= L'f') {
            value |= static_cast<unsigned>(ch - L'a' + 10);
        } else if (ch >= L'A' && ch <= L'F') {
            value |= static_cast<unsigned>(ch - L'A' + 10);
        } else {
            return false;
        }
    }
    color = RGB((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    return true;
}

std::wstring ColorToHex(COLORREF color) {
    wchar_t buffer[8] = {};
    swprintf_s(buffer, L"#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    return buffer;
}

void ClampZone(NormRect& zone) {
    constexpr double kMin = 0.05;
    if (zone.w < kMin) zone.w = kMin;
    if (zone.h < kMin) zone.h = kMin;
    if (zone.x < 0) zone.x = 0;
    if (zone.y < 0) zone.y = 0;
    if (zone.x + zone.w > 1.0) zone.x = std::max(0.0, 1.0 - zone.w);
    if (zone.y + zone.h > 1.0) zone.y = std::max(0.0, 1.0 - zone.h);
    if (zone.x + zone.w > 1.0) zone.w = 1.0 - zone.x;
    if (zone.y + zone.h > 1.0) zone.h = 1.0 - zone.y;
}

std::wstring NextCustomId(const AppSettings& settings) {
    for (int number = 1;; ++number) {
        std::wstring id = L"custom-" + std::to_wstring(number);
        if (!settings.FindLayout(id)) {
            return id;
        }
    }
}

std::wstring NextCustomName(const AppSettings& settings) {
    for (int number = 1;; ++number) {
        std::wstring name = L"Свой макет " + std::to_wstring(number);
        const bool used = std::any_of(settings.layouts.begin(), settings.layouts.end(), [&](const Layout& layout) {
            return layout.name == name;
        });
        if (!used) {
            return name;
        }
    }
}

}  // namespace layrix
