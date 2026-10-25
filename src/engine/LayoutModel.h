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

enum class ZoneDisplayMode {
    Hidden,
    OnTop,
    OnDesktop
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

inline constexpr std::wstring_view kSystemColor = L"system";

struct KeyChord {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool win = false;
    UINT vk = 0;
};

inline KeyChord DefaultSnapHold() {
    KeyChord chord;
    chord.shift = true;
    return chord;
}

struct AppSettings {
    std::wstring highlightColor{kSystemColor};
    std::wstring accentColor{kSystemColor};
    KeyChord snapHold = DefaultSnapHold();
    std::wstring language{L"ru"};
    bool overrideWinArrows = true;
    bool zonesEnabled = true;
    ZoneDisplayMode zoneDisplay = ZoneDisplayMode::OnTop;
    std::vector<Layout> layouts;
    std::vector<std::pair<std::wstring, std::wstring>> monitorLayouts;

    const Layout* FindLayout(std::wstring_view id) const;
    Layout* FindLayout(std::wstring_view id);
    std::wstring LayoutForMonitor(std::wstring_view key, std::wstring_view device) const;
    void SetMonitorLayout(std::wstring key, std::wstring layoutId);
};

std::wstring_view LayoutTypeToString(LayoutType type);
std::optional<LayoutType> LayoutTypeFromString(std::wstring_view text);
std::wstring_view ZoneDisplayToString(ZoneDisplayMode mode);
std::optional<ZoneDisplayMode> ZoneDisplayFromString(std::wstring_view text);
std::wstring ZoneDisplayTitle(ZoneDisplayMode mode);
std::wstring ZoneDisplayHint(ZoneDisplayMode mode);

std::vector<NormRect> BuildNormalizedZones(const Layout& layout);
RECT ZoneToPixels(const NormRect& zone, const RECT& work, int spacing);
RECT UnionRects(const std::vector<RECT>& rects);

bool IsSystemColorToken(std::wstring_view text);
COLORREF SystemAccentColor();
COLORREF ResolveColor(std::wstring_view text);
bool ParseHexColor(std::wstring_view text, COLORREF& color);
bool ParseColor(std::wstring_view text, COLORREF& color);
std::wstring ColorToHex(COLORREF color);
std::wstring NormalizeColorSetting(std::wstring_view text);
void ClampZone(NormRect& zone);

std::wstring NextCustomId(const AppSettings& settings);
std::wstring NextCustomName(const AppSettings& settings);

bool KeyChordEmpty(const KeyChord& chord);
bool KeyChordEqual(const KeyChord& left, const KeyChord& right);
bool KeyChordHeld(const KeyChord& chord);
bool KeyChordExtendHeld(const KeyChord& snap);
std::wstring FormatKeyChord(const KeyChord& chord);
std::wstring FormatSnapHint(const KeyChord& snap);
std::wstring FormatSnapFooter(const KeyChord& snap);
bool ParseKeyChord(std::wstring_view text, KeyChord& chord);
KeyChord KeyChordFromVirtualKey(UINT vk);

}  // namespace layrix
