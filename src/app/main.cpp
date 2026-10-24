#include "app/SelfTest.h"
#include "app/TrayApp.h"

#include "engine/Messages.h"
#include "engine/StartupRegistration.h"
#include "engine/StartupLog.h"

#include <windows.h>
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool selfTest = false;
    bool openEditor = false;
    bool openSettings = false;
    bool startup = false;
    for (int index = 1; argv && index < argc; ++index) {
        if (wcscmp(argv[index], L"--self-test") == 0) {
            selfTest = true;
        }
        if (wcscmp(argv[index], L"--editor") == 0) {
            openEditor = true;
        }
        if (wcscmp(argv[index], L"--settings") == 0) {
            openSettings = true;
        }
        if (wcscmp(argv[index], L"--startup") == 0) startup = true;
    }
    if (argv) {
        LocalFree(argv);
    }
    if (selfTest) {
        return layrix::RunSelfTest();
    }
    if (!startup && !openEditor && !openSettings) {
        openEditor = true;
    }
    layrix::StartupLog(startup ? "Windows startup launch" : "manual launch");

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\Layrix");
    if (!mutex) {
        layrix::StartupLog("single-instance mutex failed: " + std::to_string(GetLastError()));
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        layrix::StartupLog("existing instance found; exiting duplicate");
        if (HWND existing = startup ? nullptr : FindWindowW(layrix::kEngineClass, L"Layrix")) {
            PostMessageW(existing, layrix::kMsgOpenEditor, openSettings ? 1 : 0, 0);
        }
        CloseHandle(mutex);
        return 0;
    }

    const auto startupState = layrix::ReadStartup();
    if (startupState.enabled && startupState.legacy) {
        const auto error = layrix::SetStartup(true);
        layrix::StartupLog("legacy startup migration result=" + std::to_string(error));
    }

    layrix::TrayApp app;
    int code = 1;
    try {
        code = app.Run(openEditor, openSettings);
    } catch (const std::exception& error) {
        layrix::StartupLog(std::string{"startup failed: "} + error.what());
    } catch (...) {
        layrix::StartupLog("startup failed: unknown exception");
    }
    layrix::StartupLog("process exit code=" + std::to_string(code));
    CloseHandle(mutex);
    return code;
}
