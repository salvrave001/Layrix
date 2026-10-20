#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace layrix {
inline constexpr wchar_t kStartupKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

inline std::wstring StartupCommand() {
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size()) return {};
    executable.resize(length);
    // No --editor: launch the zone engine quietly in the notification area.
    return L"\"" + executable + L"\"";
}

struct StartupState { bool enabled = false; LSTATUS error = ERROR_SUCCESS; };

inline StartupState ReadStartup(const wchar_t* keyPath = kStartupKey) {
    HKEY key = nullptr;
    LSTATUS error = RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_QUERY_VALUE, &key);
    if (error == ERROR_FILE_NOT_FOUND) return {};
    if (error != ERROR_SUCCESS) return {false, error};
    DWORD type = 0, size = 0;
    error = RegQueryValueExW(key, L"Layrix", nullptr, &type, nullptr, &size);
    if (error == ERROR_FILE_NOT_FOUND) { RegCloseKey(key); return {}; }
    if (error != ERROR_SUCCESS || type != REG_SZ || size > 65536) {
        RegCloseKey(key);
        return {false, error != ERROR_SUCCESS ? error : ERROR_INVALID_DATA};
    }
    std::vector<wchar_t> value(size / sizeof(wchar_t) + 1, L'\0');
    error = RegQueryValueExW(key, L"Layrix", nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &size);
    RegCloseKey(key);
    return {error == ERROR_SUCCESS && _wcsicmp(value.data(), StartupCommand().c_str()) == 0, error};
}

inline LSTATUS SetStartup(bool enabled, const wchar_t* keyPath = kStartupKey) {
    HKEY key = nullptr;
    if (!enabled) {
        LSTATUS error = RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_SET_VALUE, &key);
        if (error == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
        if (error != ERROR_SUCCESS) return error;
        error = RegDeleteValueW(key, L"Layrix");
        RegCloseKey(key);
        return error == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : error;
    }
    const auto command = StartupCommand();
    if (command.empty()) return ERROR_BAD_PATHNAME;
    LSTATUS error = RegCreateKeyExW(HKEY_CURRENT_USER, keyPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (error != ERROR_SUCCESS) return error;
    error = RegSetValueExW(key, L"Layrix", 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return error;
}
}
