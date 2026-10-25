#include "engine/LayoutStore.h"

#include "engine/MonitorService.h"
#include "engine/UiStrings.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace layrix {
namespace {

class FileLock {
public:
    FileLock() {
        handle_ = CreateMutexW(nullptr, FALSE, L"Local\\Layrix.Settings");
        if (handle_) {
            WaitForSingleObject(handle_, 5000);
        }
    }

    ~FileLock() {
        if (handle_) {
            ReleaseMutex(handle_);
            CloseHandle(handle_);
        }
    }

    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

private:
    HANDLE handle_ = nullptr;
};

std::string ToUtf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size > 0 ? size : 0, '\0');
    if (size > 0) {
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
    }
    return out;
}

std::wstring FromUtf8(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(size > 0 ? size : 0, L'\0');
    if (size > 0) {
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    }
    return out;
}

void EnsureBuiltins(AppSettings& settings) {
    const AppSettings defaults = MakeDefaultSettings();
    std::vector<Layout> merged;
    merged.reserve(settings.layouts.size() + defaults.layouts.size());
    for (const Layout& builtin : defaults.layouts) {
        if (const Layout* existing = settings.FindLayout(builtin.id)) {
            merged.push_back(*existing);
        } else {
            merged.push_back(builtin);
        }
    }
    for (const Layout& layout : settings.layouts) {
        if (!layout.builtin) {
            merged.push_back(layout);
        }
    }
    settings.layouts = std::move(merged);
    if (settings.monitorLayouts.empty()) {
        settings.monitorLayouts = defaults.monitorLayouts;
    }
}

}  // namespace

AppSettings MakeDefaultSettings() {
    AppSettings settings;
    settings.layouts = {
        {L"none", L"Без макета", LayoutType::None, 1, 1, 0, {}, true},
        {L"focus", L"Фокус", LayoutType::Focus, 1, 1, 8, {}, true},
        {L"columns", L"Столбцы", LayoutType::Columns, 3, 1, 8, {}, true},
        {L"rows", L"Строки", LayoutType::Rows, 1, 3, 8, {}, true},
        {L"grid", L"Сетка", LayoutType::Grid, 3, 2, 8, {}, true},
        {L"priority", L"Приоритетная сетка", LayoutType::PriorityGrid, 1, 1, 8, {}, true},
    };
    for (const MonitorInfo& monitor : EnumerateMonitors()) {
        settings.SetMonitorLayout(monitor.key, L"columns");
    }
    return settings;
}

LayoutStore::LayoutStore(std::filesystem::path path) : path_(std::move(path)) {}

std::filesystem::path LayoutStore::BesideExecutable() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path() / L"layouts.json";
}

AppSettings LayoutStore::Load() {
    FileLock lock;
    return LoadUnlocked();
}

void LayoutStore::Save(const AppSettings& settings) {
    FileLock lock;
    SaveUnlocked(settings);
}

void LayoutStore::Update(const std::function<void(AppSettings&)>& mutator) {
    FileLock lock;
    AppSettings settings = LoadUnlocked();
    mutator(settings);
    SaveUnlocked(settings);
}

AppSettings LayoutStore::LoadUnlocked() {
    std::error_code error;
    if (!std::filesystem::exists(path_, error)) {
        AppSettings settings = MakeDefaultSettings();
        SaveUnlocked(settings);
        return settings;
    }

    try {
        std::ifstream input(path_, std::ios::binary);
        if (!input) {
            throw std::runtime_error("open");
        }
        const nlohmann::json document = nlohmann::json::parse(input);
        AppSettings settings;
        if (document.contains("highlightColor")) {
            settings.highlightColor = FromUtf8(document["highlightColor"].get<std::string>());
        }
        if (document.contains("accentColor")) {
            settings.accentColor = FromUtf8(document["accentColor"].get<std::string>());
        }
        if (document.contains("overrideWinArrows")) {
            settings.overrideWinArrows = document["overrideWinArrows"].get<bool>();
        }
        if (document.contains("zonesEnabled")) {
            settings.zonesEnabled = document["zonesEnabled"].get<bool>();
        }
        if (document.contains("language") && document["language"].is_string()) {
            settings.language = FromUtf8(document["language"].get<std::string>());
        }
        if (document.contains("zoneDisplay")) {
            if (const auto mode = ZoneDisplayFromString(FromUtf8(document["zoneDisplay"].get<std::string>()))) {
                settings.zoneDisplay = *mode;
            }
        } else if (document.contains("showZonesOnTop")) {
            settings.zoneDisplay = document["showZonesOnTop"].get<bool>()
                ? ZoneDisplayMode::OnTop
                : ZoneDisplayMode::Hidden;
        }
        if (document.contains("snapHold")) {
            KeyChord parsed{};
            if (ParseKeyChord(FromUtf8(document["snapHold"].get<std::string>()), parsed)) {
                settings.snapHold = parsed;
            }
        }
        if (document.contains("layouts")) {
            for (const auto& item : document["layouts"]) {
                const auto type = LayoutTypeFromString(FromUtf8(item.value("type", "columns")));
                if (!type) {
                    continue;
                }
                Layout layout;
                layout.id = FromUtf8(item.value("id", ""));
                layout.name = FromUtf8(item.value("name", ""));
                layout.type = *type;
                layout.columns = std::clamp(item.value("columns", 3), 1, 24);
                layout.rows = std::clamp(item.value("rows", 2), 1, 24);
                layout.spacing = std::clamp(item.value("spacing", 8), 0, 64);
                layout.builtin = item.value("builtin", false);
                if (item.contains("zones")) {
                    for (const auto& zone : item["zones"]) {
                        layout.zones.push_back({
                            zone.value("x", 0.0),
                            zone.value("y", 0.0),
                            zone.value("w", 0.0),
                            zone.value("h", 0.0),
                        });
                    }
                }
                if (!layout.id.empty()) {
                    settings.layouts.push_back(std::move(layout));
                }
            }
        }
        if (document.contains("monitors") && document["monitors"].is_object()) {
            for (auto it = document["monitors"].begin(); it != document["monitors"].end(); ++it) {
                settings.monitorLayouts.emplace_back(FromUtf8(it.key()), FromUtf8(it.value().get<std::string>()));
            }
        }
        const bool legacyDefault =
            !document.contains("accentColor") &&
            (settings.highlightColor.empty() ||
                settings.highlightColor == L"#4CC2FF" ||
                settings.highlightColor == L"#4cc2ff");
        if (legacyDefault) {
            settings.highlightColor = std::wstring(kSystemColor);
        }
        settings.highlightColor = NormalizeColorSetting(settings.highlightColor);
        settings.accentColor = NormalizeColorSetting(settings.accentColor);
        settings.language = std::wstring(UiLanguageCode(ParseUiLanguage(settings.language)));
        if (settings.layouts.empty()) {
            throw std::runtime_error("empty");
        }
        EnsureBuiltins(settings);
        return settings;
    } catch (...) {
        std::filesystem::path backup = path_;
        backup += L".bak";
        std::filesystem::remove(backup, error);
        std::filesystem::rename(path_, backup, error);
        AppSettings settings = MakeDefaultSettings();
        SaveUnlocked(settings);
        return settings;
    }
}

void LayoutStore::SaveUnlocked(const AppSettings& settings) {
    nlohmann::json layouts = nlohmann::json::array();
    for (const Layout& layout : settings.layouts) {
        nlohmann::json zones = nlohmann::json::array();
        for (const NormRect& zone : layout.zones) {
            zones.push_back({{"x", zone.x}, {"y", zone.y}, {"w", zone.w}, {"h", zone.h}});
        }
        layouts.push_back({
            {"id", ToUtf8(layout.id)},
            {"name", ToUtf8(layout.name)},
            {"type", ToUtf8(LayoutTypeToString(layout.type))},
            {"columns", layout.columns},
            {"rows", layout.rows},
            {"spacing", layout.spacing},
            {"builtin", layout.builtin},
            {"zones", std::move(zones)},
        });
    }
    nlohmann::json monitors = nlohmann::json::object();
    for (const auto& [key, layoutId] : settings.monitorLayouts) {
        monitors[ToUtf8(key)] = ToUtf8(layoutId);
    }
    const nlohmann::json document = {
        {"highlightColor", ToUtf8(settings.highlightColor)},
        {"accentColor", ToUtf8(settings.accentColor)},
        {"overrideWinArrows", settings.overrideWinArrows},
        {"zonesEnabled", settings.zonesEnabled},
        {"zoneDisplay", ToUtf8(ZoneDisplayToString(settings.zoneDisplay))},
        {"snapHold", ToUtf8(FormatKeyChord(settings.snapHold))},
        {"language", ToUtf8(settings.language)},
        {"layouts", std::move(layouts)},
        {"monitors", std::move(monitors)},
    };

    std::error_code error;
    if (path_.has_parent_path()) {
        std::filesystem::create_directories(path_.parent_path(), error);
    }
    const std::filesystem::path temporary = path_.wstring() + L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << document.dump(2, ' ', false);
        output.flush();
        if (!output) {
            return;
        }
    }
    MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

}  // namespace layrix
