#include "app/SelfTest.h"
#include "app/TrayApp.h"

#include "engine/Messages.h"

#include <windows.h>
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool selfTest = false;
    bool openEditor = false;
    bool openSettings = false;
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
    }
    if (argv) {
        LocalFree(argv);
    }
    if (selfTest) {
        return layrix::RunSelfTest();
    }

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\Layrix");
    if (!mutex) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(layrix::kEngineClass, L"Layrix")) {
            PostMessageW(existing, layrix::kMsgOpenEditor, openSettings ? 1 : 0, 0);
        }
        CloseHandle(mutex);
        return 0;
    }

    layrix::TrayApp app;
    const int code = app.Run(openEditor, openSettings);
    CloseHandle(mutex);
    return code;
}
