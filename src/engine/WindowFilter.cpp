#include "engine/WindowFilter.h"

#include <dwmapi.h>

#include <string>
#include <vector>

namespace layrix {
namespace {

bool IsShellClass(const wchar_t* className) {
    const wchar_t* blocked[] = {
        L"Progman",
        L"WorkerW",
        L"Shell_TrayWnd",
        L"Shell_SecondaryTrayWnd",
        L"NotifyIconOverflowWindow",
        L"DV2ControlHost",
        L"MsgrIMEWindowClass",
        L"SysShadow",
        L"IME",
        L"MSCTFIME UI",
        L"TaskListThumbnailWnd",
        L"ForegroundStaging",
    };
    for (const wchar_t* name : blocked) {
        if (_wcsicmp(className, name) == 0) {
            return true;
        }
    }
    return false;
}

DWORD IntegrityLevel(HANDLE process) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token)) {
        return 0;
    }
    DWORD length = 0;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &length);
    if (length == 0) {
        CloseHandle(token);
        return 0;
    }
    std::vector<unsigned char> buffer(length);
    if (!GetTokenInformation(token, TokenIntegrityLevel, buffer.data(), length, &length)) {
        CloseHandle(token);
        return 0;
    }
    CloseHandle(token);
    auto* label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer.data());
    if (!label->Label.Sid) {
        return 0;
    }
    PUCHAR count = GetSidSubAuthorityCount(label->Label.Sid);
    if (!count || *count == 0) {
        return 0;
    }
    return *GetSidSubAuthority(label->Label.Sid, static_cast<DWORD>(*count - 1));
}

DWORD CurrentIntegrity() {
    static DWORD level = IntegrityLevel(GetCurrentProcess());
    return level == 0 ? SECURITY_MANDATORY_MEDIUM_RID : level;
}

bool HasHigherIntegrity(HWND hwnd) {
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == 0) {
        return true;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) {
        return true;
    }
    const DWORD level = IntegrityLevel(process);
    CloseHandle(process);
    if (level == 0) {
        return true;
    }
    return level > CurrentIntegrity();
}

}  // namespace

bool IsSnapTarget(HWND hwnd, bool allowIconic) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return false;
    }
    if (!allowIconic && IsIconic(hwnd)) {
        return false;
    }

    HWND root = GetAncestor(hwnd, GA_ROOT);
    if (root) {
        hwnd = root;
    }

    const LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    const LONG extended = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (style & WS_CHILD) {
        return false;
    }
    if ((extended & WS_EX_TOOLWINDOW) && !(extended & WS_EX_APPWINDOW)) {
        return false;
    }
    if (!(style & WS_THICKFRAME) && !(style & WS_MAXIMIZEBOX)) {
        return false;
    }

    wchar_t className[128] = {};
    GetClassNameW(hwnd, className, 128);
    if (IsShellClass(className)) {
        return false;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == GetCurrentProcessId()) {
        return false;
    }
    if (HasHigherIntegrity(hwnd)) {
        return false;
    }

    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) {
        return false;
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return false;
    }
    if ((rect.right - rect.left) < 40 || (rect.bottom - rect.top) < 40) {
        if (!allowIconic || !IsIconic(hwnd)) {
            return false;
        }
    }
    return true;
}

}  // namespace layrix
