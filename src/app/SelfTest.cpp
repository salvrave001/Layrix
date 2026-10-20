#include "app/SelfTest.h"

#include "engine/LayoutModel.h"
#include "engine/LayoutStore.h"
#include "engine/ZoneEditing.h"
#include "engine/StartupRegistration.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace layrix {
namespace {

int g_failed = 0;

void Check(bool condition, const char* text) {
    if (!condition) {
        std::cerr << "FAIL " << text << '\n';
        ++g_failed;
    }
}

bool Near(double left, double right) {
    return std::abs(left - right) < 1e-9;
}

}  // namespace

int RunSelfTest() {
    AttachConsole(ATTACH_PARENT_PROCESS);
    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);

    Layout columns;
    columns.type = LayoutType::Columns;
    columns.columns = 3;
    const auto columnZones = BuildNormalizedZones(columns);
    Check(columnZones.size() == 3, "columns count");
    Check(Near(columnZones[0].x, 0.0) && Near(columnZones[0].w, 1.0 / 3.0) && Near(columnZones[0].h, 1.0), "column 0");
    Check(Near(columnZones[2].x, 2.0 / 3.0), "column 2 x");

    Layout rows;
    rows.type = LayoutType::Rows;
    rows.rows = 3;
    const auto rowZones = BuildNormalizedZones(rows);
    Check(rowZones.size() == 3 && Near(rowZones[1].y, 1.0 / 3.0) && Near(rowZones[1].h, 1.0 / 3.0), "rows");

    Layout grid;
    grid.type = LayoutType::Grid;
    grid.columns = 3;
    grid.rows = 2;
    const auto gridZones = BuildNormalizedZones(grid);
    Check(gridZones.size() == 6, "grid count");
    Check(Near(gridZones[5].x, 2.0 / 3.0) && Near(gridZones[5].y, 0.5), "grid last");

    Layout priority;
    priority.type = LayoutType::PriorityGrid;
    const auto priorityZones = BuildNormalizedZones(priority);
    Check(priorityZones.size() == 3 && Near(priorityZones[0].w, 0.5) && Near(priorityZones[0].h, 1.0), "priority");
    Check(Near(priorityZones[2].y, 0.5), "priority bottom");

    Layout focus;
    focus.type = LayoutType::Focus;
    const auto focusZones = BuildNormalizedZones(focus);
    Check(focusZones.size() == 2, "focus count");
    Check(focusZones[1].x > focusZones[0].x && focusZones[1].x + focusZones[1].w < 1.0, "focus inner");

    Layout none;
    none.type = LayoutType::None;
    Check(BuildNormalizedZones(none).empty(), "none");

    Layout canvas;
    canvas.type = LayoutType::Canvas;
    canvas.zones = {{0.1, 0.2, 0.3, 0.4}};
    const auto canvasZones = BuildNormalizedZones(canvas);
    Check(canvasZones.size() == 1 && Near(canvasZones[0].w, 0.3), "canvas");

    const RECT work{0, 0, 300, 100};
    const RECT first = ZoneToPixels(columnZones[0], work, 0);
    const RECT second = ZoneToPixels(columnZones[1], work, 0);
    Check(first.left == 0 && first.right == 100 && first.bottom == 100, "pixel column 0");
    Check(second.left == 100 && second.right == 200, "pixel column 1");
    const RECT inset = ZoneToPixels(columnZones[0], work, 10);
    const RECT insetNext = ZoneToPixels(columnZones[1], work, 10);
    Check(inset.left == 5 && inset.right == 95, "spacing inset");
    Check(insetNext.left - inset.right == 10, "spacing gap");

    const RECT full = ZoneToPixels({0, 0, 1, 1}, RECT{0, 0, 100, 100}, 10);
    Check(full.left == 5 && full.top == 5 && full.right == 95 && full.bottom == 95, "full inset");

    const RECT united = UnionRects({RECT{5, 5, 95, 95}, RECT{105, 5, 195, 95}});
    Check(united.left == 5 && united.right == 195 && united.bottom == 95, "union");

    const std::vector<NormRect> adjacent{{0, 0, 0.5, 1}, {0.5, 0, 0.5, 1}};
    auto edited = adjacent;
    ResizeZoneBoundary(edited, adjacent, 0, true, false, 0.7);
    Check(Near(edited[0].w, 0.7) && Near(edited[1].x, 0.7) && Near(edited[1].w, 0.3), "shared desktop boundary");
    ResizeZoneBoundary(edited, adjacent, 1, true, true, 2.0);
    Check(Near(edited[0].w, 0.96) && Near(edited[1].w, 0.04), "boundary minimum size");
    const std::vector<NormRect> fragmented{{0, 0, 0.5, 1}, {0.5, 0, 0.5, 0.5}, {0.5, 0.5, 0.5, 0.5}};
    edited = fragmented;
    ResizeZoneBoundary(edited, fragmented, 1, true, true, 0.6);
    Check(Near(edited[0].w, 0.6) && Near(edited[1].x, 0.6) && Near(edited[2].x, 0.6) && Near(edited[2].w, 0.4), "fragmented shared boundary");
    const std::vector<NormRect> stacked{{0, 0, 1, 0.5}, {0, 0.5, 1, 0.5}};
    edited = stacked;
    ResizeZoneBoundary(edited, stacked, 0, false, false, 0.3);
    Check(Near(edited[0].h, 0.3) && Near(edited[1].y, 0.3) && Near(edited[1].h, 0.7), "horizontal desktop boundary");
    const std::vector<NormRect> nearEdges{{0, 0, 0.4995, 1}, {0.5, 0, 0.5, 1}};
    edited = nearEdges;
    ResizeZoneBoundary(edited, nearEdges, 1, true, true, 0.6, 0.002);
    Check(Near(edited[0].w, 0.6) && Near(edited[1].x, 0.6), "nearby canvas edges share boundary");

    COLORREF color = 0;
    Check(ParseHexColor(L"#4CC2FF", color) && GetRValue(color) == 76 && GetGValue(color) == 194 && GetBValue(color) == 255, "color");
    Check(ColorToHex(color) == L"#4CC2FF", "color hex");
    Check(!ParseHexColor(L"blue", color), "bad color");

    AppSettings names;
    names.layouts.push_back(Layout{L"custom-1", L"Свой макет 1", LayoutType::Canvas, 1, 1, 8, {}, false});
    Check(NextCustomId(names) == L"custom-2", "next id");
    Check(NextCustomName(names) == L"Свой макет 2", "next name");

    AppSettings fallback;
    fallback.monitorLayouts.emplace_back(L"\\\\.\\DISPLAY1|800x600", L"focus");
    Check(fallback.LayoutForMonitor(L"\\\\.\\DISPLAY1|1920x1080", L"\\\\.\\DISPLAY1") == L"focus", "monitor fallback");
    Check(fallback.LayoutForMonitor(L"\\\\.\\DISPLAY2|100x100", L"\\\\.\\DISPLAY2") == L"columns", "monitor default");

    const auto root = std::filesystem::temp_directory_path() / L"layrix-self-test";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root);
    LayoutStore store(root / L"layouts.json");
    AppSettings saved = MakeDefaultSettings();
    saved.highlightColor = L"#3DDC97";
    saved.overrideWinArrows = false;
    Layout custom{L"custom-9", L"Мой макет", LayoutType::Canvas, 1, 1, 12, {{0.2, 0.2, 0.5, 0.5}}, false};
    saved.layouts.push_back(custom);
    saved.SetMonitorLayout(L"\\\\.\\DISPLAY9|10x10", L"custom-9");
    store.Save(saved);
    const AppSettings loaded = store.Load();
    const Layout* found = loaded.FindLayout(L"custom-9");
    Check(loaded.highlightColor == L"#3DDC97" && !loaded.overrideWinArrows, "json settings");
    Check(found && found->name == L"Мой макет" && found->zones.size() == 1 && found->spacing == 12, "json canvas");
    Check(loaded.LayoutForMonitor(L"\\\\.\\DISPLAY9|10x10", L"\\\\.\\DISPLAY9") == L"custom-9", "json monitor");
    Check(loaded.FindLayout(L"columns") != nullptr, "builtins kept");

    {
        std::ofstream broken(root / L"broken.json", std::ios::binary);
        broken << "{";
    }
    LayoutStore brokenStore(root / L"broken.json");
    const AppSettings recovered = brokenStore.Load();
    Check(recovered.FindLayout(L"columns") != nullptr, "corrupt recovery");
    Check(std::filesystem::exists(root / L"broken.json.bak"), "corrupt backup");

    const AppSettings defaults = MakeDefaultSettings();
    Check(defaults.FindLayout(L"priority") && defaults.FindLayout(L"none"), "defaults");

    // Exercise the registry implementation in a private key, never Windows Run.
    const std::wstring startupTestKey = L"Software\\LayrixSelfTest-" + std::to_wstring(GetCurrentProcessId());
    Check(!ReadStartup(startupTestKey.c_str()).enabled, "startup initially disabled");
    Check(SetStartup(true, startupTestKey.c_str()) == ERROR_SUCCESS, "enable startup registration");
    Check(ReadStartup(startupTestKey.c_str()).enabled, "startup registration readback");
    Check(StartupCommand().front() == L'\"' && StartupCommand().back() == L'\"' &&
        StartupCommand().find(L"--editor") == std::wstring::npos, "quoted background startup command");
    Check(SetStartup(false, startupTestKey.c_str()) == ERROR_SUCCESS, "disable startup registration");
    Check(!ReadStartup(startupTestKey.c_str()).enabled, "startup disabled readback");
    Check(SetStartup(false, startupTestKey.c_str()) == ERROR_SUCCESS, "disable startup is idempotent");
    Check(RegDeleteKeyW(HKEY_CURRENT_USER, startupTestKey.c_str()) == ERROR_SUCCESS, "startup test registry cleanup");

    if (g_failed == 0) {
        std::cout << "SELFTEST OK\n";
    } else {
        std::cout << "SELFTEST FAILED " << g_failed << "\n";
    }
    std::cout.flush();
    return g_failed == 0 ? 0 : 1;
}

}  // namespace layrix
