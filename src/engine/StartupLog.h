#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>

namespace layrix {
inline void StartupLog(const std::string& event) noexcept {
    try {
        wchar_t localAppData[32768]{};
        if (!GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, 32768)) return;
        const auto folder = std::filesystem::path(localAppData) / L"Layrix" / L"logs";
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        if (error) return;
        const auto path = folder / L"startup.log";
        const auto size = std::filesystem::file_size(path, error);
        std::ofstream log(path, !error && size > 128 * 1024 ? std::ios::trunc : std::ios::app);
        SYSTEMTIME now{};
        GetLocalTime(&now);
        char stamp[64]{};
        sprintf_s(stamp, "%04d-%02d-%02d %02d:%02d:%02d pid=%lu ", now.wYear, now.wMonth,
            now.wDay, now.wHour, now.wMinute, now.wSecond, GetCurrentProcessId());
        log << stamp << event << '\n';
    } catch (...) {}
}
}
