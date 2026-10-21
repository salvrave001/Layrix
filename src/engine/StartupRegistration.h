#pragma once
#include <windows.h>
#include <filesystem>
#include <string>

namespace layrix {
inline constexpr wchar_t kStartupKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
struct StartupPaths {
    std::wstring legacyRunKey = kStartupKey;
    std::filesystem::path shortcut;
    LSTATUS error = ERROR_SUCCESS;
};
struct StartupState {
    bool enabled = false;
    LSTATUS error = ERROR_SUCCESS;
    bool legacy = false;
};
StartupPaths UserStartupPaths();
std::filesystem::path StartupExecutable();
std::wstring StartupCommand();
StartupState ReadStartup(const StartupPaths& paths = UserStartupPaths());
LSTATUS SetStartup(bool enabled, const StartupPaths& paths = UserStartupPaths());
}
