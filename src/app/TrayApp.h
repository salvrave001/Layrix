#pragma once

#include <windows.h>
#include <shellapi.h>
#include <objbase.h>

#include <memory>

namespace layrix {

class SnapEngine;

class TrayApp {
public:
    TrayApp();
    ~TrayApp();
    int Run(bool openEditor = false, bool openSettings = false);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT Handle(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void AddTray();
    void RemoveTray();
    void ShowMenu();
    void OpenEditor(bool settings = false);

    HWND hwnd_ = nullptr;
    NOTIFYICONDATAW icon_{};
    UINT taskbarCreated_ = 0;
    bool trayRegistered_ = false;
    bool trayFailureLogged_ = false;
    std::unique_ptr<SnapEngine> engine_;
    HMODULE editorModule_ = nullptr;
    void(__stdcall* showEditor_)() = nullptr;
    void(__stdcall* showSettings_)() = nullptr;
};

}  // namespace layrix
