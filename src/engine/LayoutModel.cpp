#include "engine/LayoutModel.h"

#include <dwmapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <vector>

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

std::wstring_view ZoneDisplayToString(ZoneDisplayMode mode) {
    switch (mode) {
    case ZoneDisplayMode::Hidden:
        return L"hidden";
    case ZoneDisplayMode::OnTop:
        return L"ontop";
    case ZoneDisplayMode::OnDesktop:
        return L"desktop";
    }
    return L"ontop";
}

std::optional<ZoneDisplayMode> ZoneDisplayFromString(std::wstring_view text) {
    if (text == L"hidden" || text == L"invisible" || text == L"none") {
        return ZoneDisplayMode::Hidden;
    }
    if (text == L"ontop" || text == L"top" || text == L"overlay") {
        return ZoneDisplayMode::OnTop;
    }
    if (text == L"desktop" || text == L"wallpaper" || text == L"behind") {
        return ZoneDisplayMode::OnDesktop;
    }
    return std::nullopt;
}

std::wstring_view ZoneDisplayTitle(ZoneDisplayMode mode) {
    switch (mode) {
    case ZoneDisplayMode::Hidden:
        return L"Невидимые области";
    case ZoneDisplayMode::OnTop:
        return L"Зоны поверх";
    case ZoneDisplayMode::OnDesktop:
        return L"Зоны на рабочем столе";
    }
    return L"Зоны поверх";
}

std::wstring_view ZoneDisplayHint(ZoneDisplayMode mode) {
    switch (mode) {
    case ZoneDisplayMode::Hidden:
        return L"При перетаскивании окна зоны не рисуются. Привязка по клавише всё равно работает.";
    case ZoneDisplayMode::OnTop:
        return L"Во время перетаскивания контуры зон рисуются поверх других окон и не перехватывают мышь.";
    case ZoneDisplayMode::OnDesktop:
        return L"Во время перетаскивания зоны видны на рабочем столе и остаются под окнами приложений.";
    }
    return L"";
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

namespace {

std::wstring TrimCopy(std::wstring_view text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::iswspace(text[begin])) {
        ++begin;
    }
    while (end > begin && std::iswspace(text[end - 1])) {
        --end;
    }
    return std::wstring(text.substr(begin, end - begin));
}

bool EqualsIgnoreCase(std::wstring_view left, std::wstring_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (size_t index = 0; index < left.size(); ++index) {
        if (std::towlower(left[index]) != std::towlower(right[index])) {
            return false;
        }
    }
    return true;
}

bool HexDigit(wchar_t ch, unsigned& value) {
    if (ch >= L'0' && ch <= L'9') {
        value = static_cast<unsigned>(ch - L'0');
        return true;
    }
    if (ch >= L'a' && ch <= L'f') {
        value = static_cast<unsigned>(ch - L'a' + 10);
        return true;
    }
    if (ch >= L'A' && ch <= L'F') {
        value = static_cast<unsigned>(ch - L'A' + 10);
        return true;
    }
    return false;
}

bool ParseByte(std::wstring_view text, int& value) {
    if (text.empty() || text.size() > 3) {
        return false;
    }
    int parsed = 0;
    for (wchar_t ch : text) {
        if (ch < L'0' || ch > L'9') {
            return false;
        }
        parsed = parsed * 10 + (ch - L'0');
    }
    if (parsed > 255) {
        return false;
    }
    value = parsed;
    return true;
}

std::vector<std::wstring> SplitColorParts(std::wstring_view text) {
    std::vector<std::wstring> parts;
    std::wstring current;
    for (wchar_t ch : text) {
        if (ch == L',' || std::iswspace(ch) || ch == L';' || ch == L'/') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

}  // namespace

bool IsSystemColorToken(std::wstring_view text) {
    const std::wstring trimmed = TrimCopy(text);
    return trimmed.empty() ||
        EqualsIgnoreCase(trimmed, kSystemColor) ||
        EqualsIgnoreCase(trimmed, L"auto") ||
        EqualsIgnoreCase(trimmed, L"windows");
}

COLORREF SystemAccentColor() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD value = 0;
        DWORD size = sizeof(value);
        DWORD type = 0;
        const LSTATUS status = RegQueryValueExW(key, L"AccentColor", nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(key);
        if (status == ERROR_SUCCESS && type == REG_DWORD) {
            return RGB(GetRValue(value), GetGValue(value), GetBValue(value));
        }
    }

    DWORD colorization = 0;
    BOOL opaque = FALSE;
    if (SUCCEEDED(DwmGetColorizationColor(&colorization, &opaque))) {
        return RGB((colorization >> 16) & 0xFF, (colorization >> 8) & 0xFF, colorization & 0xFF);
    }
    return RGB(0, 120, 212);
}

COLORREF ResolveColor(std::wstring_view text) {
    if (IsSystemColorToken(text)) {
        return SystemAccentColor();
    }
    COLORREF color = 0;
    if (ParseColor(text, color)) {
        return color;
    }
    return SystemAccentColor();
}

bool ParseHexColor(std::wstring_view text, COLORREF& color) {
    std::wstring trimmed = TrimCopy(text);
    std::wstring_view hex = trimmed;
    if (!hex.empty() && hex.front() == L'#') {
        hex.remove_prefix(1);
    }
    if (hex.size() == 3) {
        unsigned digits[3] = {};
        for (int index = 0; index < 3; ++index) {
            if (!HexDigit(hex[static_cast<size_t>(index)], digits[index])) {
                return false;
            }
        }
        color = RGB(
            static_cast<BYTE>(digits[0] * 17),
            static_cast<BYTE>(digits[1] * 17),
            static_cast<BYTE>(digits[2] * 17));
        return true;
    }
    if (hex.size() != 6) {
        return false;
    }
    unsigned value = 0;
    for (wchar_t ch : hex) {
        unsigned digit = 0;
        if (!HexDigit(ch, digit)) {
            return false;
        }
        value = (value << 4) | digit;
    }
    color = RGB((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    return true;
}

bool ParseColor(std::wstring_view text, COLORREF& color) {
    const std::wstring trimmed = TrimCopy(text);
    if (trimmed.empty() || IsSystemColorToken(trimmed)) {
        return false;
    }
    if (ParseHexColor(trimmed, color)) {
        return true;
    }

    std::wstring_view body = trimmed;
    if (body.size() >= 4) {
        const bool rgb = EqualsIgnoreCase(body.substr(0, 3), L"rgb") && (body[3] == L'(' || body[3] == L'A' || body[3] == L'a');
        if (rgb) {
            const size_t open = trimmed.find(L'(');
            const size_t close = trimmed.rfind(L')');
            if (open != std::wstring::npos && close != std::wstring::npos && close > open) {
                body = std::wstring_view(trimmed).substr(open + 1, close - open - 1);
            }
        }
    }

    const auto parts = SplitColorParts(body);
    if (parts.size() < 3) {
        return false;
    }
    int red = 0;
    int green = 0;
    int blue = 0;
    if (!ParseByte(parts[0], red) || !ParseByte(parts[1], green) || !ParseByte(parts[2], blue)) {
        return false;
    }
    color = RGB(red, green, blue);
    return true;
}

std::wstring ColorToHex(COLORREF color) {
    wchar_t buffer[8] = {};
    swprintf_s(buffer, L"#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    return buffer;
}

std::wstring NormalizeColorSetting(std::wstring_view text) {
    if (IsSystemColorToken(text)) {
        return std::wstring(kSystemColor);
    }
    COLORREF color = 0;
    if (ParseColor(text, color)) {
        return ColorToHex(color);
    }
    return std::wstring(kSystemColor);
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

namespace {

bool IsModifierVk(UINT vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
        vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
        vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
        vk == VK_LWIN || vk == VK_RWIN;
}

bool AsyncDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

void NormalizeKeyChord(KeyChord& chord) {
    if (chord.vk == VK_SHIFT || chord.vk == VK_LSHIFT || chord.vk == VK_RSHIFT) {
        chord.shift = true;
        chord.vk = 0;
    } else if (chord.vk == VK_CONTROL || chord.vk == VK_LCONTROL || chord.vk == VK_RCONTROL) {
        chord.ctrl = true;
        chord.vk = 0;
    } else if (chord.vk == VK_MENU || chord.vk == VK_LMENU || chord.vk == VK_RMENU) {
        chord.alt = true;
        chord.vk = 0;
    } else if (chord.vk == VK_LWIN || chord.vk == VK_RWIN) {
        chord.win = true;
        chord.vk = 0;
    }
}

std::wstring VirtualKeyLabel(UINT vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }
    if (vk >= '0' && vk <= '9') {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    }
    switch (vk) {
    case VK_SPACE: return L"Space";
    case VK_TAB: return L"Tab";
    case VK_CAPITAL: return L"CapsLock";
    case VK_OEM_PLUS: return L"+";
    case VK_OEM_MINUS: return L"-";
    case VK_OEM_COMMA: return L",";
    case VK_OEM_PERIOD: return L".";
    case VK_OEM_1: return L";";
    case VK_OEM_2: return L"/";
    case VK_OEM_3: return L"`";
    case VK_OEM_4: return L"[";
    case VK_OEM_5: return L"\\";
    case VK_OEM_6: return L"]";
    case VK_OEM_7: return L"'";
    default: break;
    }
    const UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    wchar_t name[64] = {};
    if (scan && GetKeyNameTextW(static_cast<LONG>(scan) << 16, name, 64) > 0) {
        return name;
    }
    return L"VK" + std::to_wstring(vk);
}

bool ParseKeyToken(std::wstring_view token, KeyChord& chord) {
    if (EqualsIgnoreCase(token, L"Ctrl") || EqualsIgnoreCase(token, L"Control") || EqualsIgnoreCase(token, L"Ctl")) {
        chord.ctrl = true;
        return true;
    }
    if (EqualsIgnoreCase(token, L"Alt") || EqualsIgnoreCase(token, L"Menu")) {
        chord.alt = true;
        return true;
    }
    if (EqualsIgnoreCase(token, L"Shift")) {
        chord.shift = true;
        return true;
    }
    if (EqualsIgnoreCase(token, L"Win") || EqualsIgnoreCase(token, L"Windows") || EqualsIgnoreCase(token, L"Super")) {
        chord.win = true;
        return true;
    }
    if (token.size() == 1) {
        const wchar_t ch = static_cast<wchar_t>(std::towupper(token[0]));
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) {
            chord.vk = static_cast<UINT>(ch);
            return true;
        }
    }
    if (token.size() >= 2 && (token[0] == L'F' || token[0] == L'f')) {
        int number = 0;
        if (ParseByte(token.substr(1), number) && number >= 1 && number <= 24) {
            chord.vk = VK_F1 + static_cast<UINT>(number - 1);
            return true;
        }
    }
    static constexpr std::pair<const wchar_t*, UINT> named[] = {
        {L"Space", VK_SPACE}, {L"Tab", VK_TAB}, {L"CapsLock", VK_CAPITAL},
    };
    for (const auto& [name, vk] : named) {
        if (EqualsIgnoreCase(token, name)) {
            chord.vk = vk;
            return true;
        }
    }
    return false;
}

}  // namespace

bool KeyChordEmpty(const KeyChord& chord) {
    return !chord.ctrl && !chord.alt && !chord.shift && !chord.win && chord.vk == 0;
}

bool KeyChordEqual(const KeyChord& left, const KeyChord& right) {
    return left.ctrl == right.ctrl && left.alt == right.alt && left.shift == right.shift &&
        left.win == right.win && left.vk == right.vk;
}

bool KeyChordHeld(const KeyChord& chord) {
    if (KeyChordEmpty(chord)) {
        return false;
    }
    if (chord.ctrl && !AsyncDown(VK_CONTROL)) {
        return false;
    }
    if (chord.alt && !AsyncDown(VK_MENU)) {
        return false;
    }
    if (chord.shift && !AsyncDown(VK_SHIFT)) {
        return false;
    }
    if (chord.win && !AsyncDown(VK_LWIN) && !AsyncDown(VK_RWIN)) {
        return false;
    }
    if (chord.vk && !IsModifierVk(chord.vk) && !AsyncDown(static_cast<int>(chord.vk))) {
        return false;
    }
    return true;
}

bool KeyChordExtendHeld(const KeyChord& snap) {
    if (!snap.ctrl) {
        return AsyncDown(VK_CONTROL);
    }
    if (!snap.shift) {
        return AsyncDown(VK_SHIFT);
    }
    if (!snap.alt) {
        return AsyncDown(VK_MENU);
    }
    return false;
}

std::wstring FormatKeyChord(const KeyChord& chord) {
    KeyChord normalized = chord;
    NormalizeKeyChord(normalized);
    std::wstring text;
    auto append = [&](std::wstring const& part) {
        if (!text.empty()) {
            text += L" + ";
        }
        text += part;
    };
    if (normalized.ctrl) {
        append(L"Ctrl");
    }
    if (normalized.alt) {
        append(L"Alt");
    }
    if (normalized.shift) {
        append(L"Shift");
    }
    if (normalized.win) {
        append(L"Win");
    }
    if (normalized.vk && !IsModifierVk(normalized.vk)) {
        append(VirtualKeyLabel(normalized.vk));
    }
    return text.empty() ? L"Shift" : text;
}

std::wstring FormatSnapHint(const KeyChord& snap) {
    const std::wstring hold = FormatKeyChord(snap);
    std::wstring extra = L"Ctrl";
    if (snap.ctrl && !snap.shift) {
        extra = L"Shift";
    } else if (snap.ctrl && snap.shift && !snap.alt) {
        extra = L"Alt";
    }
    return L"Удерживайте " + hold + L" при перетаскивании окна. " + hold + L" + " + extra +
        L" позволяет занять несколько зон.";
}

std::wstring FormatSnapFooter(const KeyChord& snap) {
    return L"Удерживайте " + FormatKeyChord(snap) + L" при перетаскивании окна в зону";
}

bool ParseKeyChord(std::wstring_view text, KeyChord& chord) {
    KeyChord parsed{};
    std::wstring current;
    auto flush = [&] {
        const std::wstring token = TrimCopy(current);
        current.clear();
        return token.empty() || ParseKeyToken(token, parsed);
    };
    for (wchar_t ch : text) {
        if (ch == L'+') {
            if (!flush()) {
                return false;
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!flush()) {
        return false;
    }
    NormalizeKeyChord(parsed);
    if (KeyChordEmpty(parsed)) {
        return false;
    }
    chord = parsed;
    return true;
}

KeyChord KeyChordFromVirtualKey(UINT vk) {
    KeyChord chord;
    chord.ctrl = AsyncDown(VK_CONTROL);
    chord.alt = AsyncDown(VK_MENU);
    chord.shift = AsyncDown(VK_SHIFT);
    chord.win = AsyncDown(VK_LWIN) || AsyncDown(VK_RWIN);
    chord.vk = vk;
    NormalizeKeyChord(chord);
    if (KeyChordEmpty(chord) && IsModifierVk(vk)) {
        if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
            chord.shift = true;
        } else if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) {
            chord.ctrl = true;
        } else if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU) {
            chord.alt = true;
        } else {
            chord.win = true;
        }
    }
    return chord;
}

}  // namespace layrix
