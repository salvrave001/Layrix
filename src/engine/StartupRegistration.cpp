#include "engine/StartupRegistration.h"
#include <shlobj.h>
#include <shobjidl.h>
#include <memory>
#include <vector>

namespace layrix {
namespace {
struct Apartment {
    HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ~Apartment() { if (SUCCEEDED(result)) CoUninitialize(); }
};
struct ReleaseCom {
    void operator()(IUnknown* object) const { if (object) object->Release(); }
};
LSTATUS Win32Error(HRESULT result) {
    return HRESULT_FACILITY(result) == FACILITY_WIN32 ? HRESULT_CODE(result) : ERROR_GEN_FAILURE;
}
bool Missing(LSTATUS error) { return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND; }

StartupState ReadLegacy(const std::wstring& path) {
    HKEY key = nullptr;
    LSTATUS error = RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_QUERY_VALUE, &key);
    if (Missing(error)) return {};
    if (error != ERROR_SUCCESS) return {false, error};
    DWORD type = 0, size = 0;
    error = RegQueryValueExW(key, L"Layrix", nullptr, &type, nullptr, &size);
    if (error != ERROR_SUCCESS || type != REG_SZ || size > 65536) {
        RegCloseKey(key);
        return {false, Missing(error) ? ERROR_SUCCESS : (error ? error : ERROR_INVALID_DATA)};
    }
    std::vector<wchar_t> value(size / sizeof(wchar_t) + 1, L'\0');
    error = RegQueryValueExW(key, L"Layrix", nullptr, nullptr, reinterpret_cast<BYTE*>(value.data()), &size);
    RegCloseKey(key);
    const auto oldCommand = L"\"" + StartupExecutable().wstring() + L"\"";
    const bool matches = _wcsicmp(value.data(), oldCommand.c_str()) == 0 ||
        _wcsicmp(value.data(), StartupCommand().c_str()) == 0;
    return {error == ERROR_SUCCESS && matches, error, matches};
}

LSTATUS RemoveLegacy(const std::wstring& path) {
    HKEY key = nullptr;
    LSTATUS error = RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_SET_VALUE, &key);
    if (Missing(error)) return ERROR_SUCCESS;
    if (error != ERROR_SUCCESS) return error;
    error = RegDeleteValueW(key, L"Layrix");
    RegCloseKey(key);
    return Missing(error) ? ERROR_SUCCESS : error;
}

StartupState ReadShortcut(const std::filesystem::path& path) {
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        const auto error = GetLastError();
        return {false, Missing(error) ? ERROR_SUCCESS : static_cast<LSTATUS>(error)};
    }
    Apartment apartment;
    if (FAILED(apartment.result) && apartment.result != RPC_E_CHANGED_MODE) return {false, Win32Error(apartment.result)};
    IShellLinkW* rawLink = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&rawLink));
    if (FAILED(hr)) return {false, Win32Error(hr)};
    std::unique_ptr<IShellLinkW, ReleaseCom> link(rawLink);
    IPersistFile* rawFile = nullptr;
    hr = link->QueryInterface(IID_PPV_ARGS(&rawFile));
    if (FAILED(hr)) return {false, Win32Error(hr)};
    std::unique_ptr<IPersistFile, ReleaseCom> file(rawFile);
    hr = file->Load(path.c_str(), STGM_READ);
    if (FAILED(hr)) return {false, Win32Error(hr)};
    wchar_t target[32768]{}, directory[32768]{}, arguments[128]{};
    if (FAILED(link->GetPath(target, 32768, nullptr, SLGP_RAWPATH)) ||
        FAILED(link->GetWorkingDirectory(directory, 32768)) || FAILED(link->GetArguments(arguments, 128)))
        return {false, ERROR_INVALID_DATA};
    const auto executable = StartupExecutable();
    return {_wcsicmp(target, executable.c_str()) == 0 &&
        _wcsicmp(directory, executable.parent_path().c_str()) == 0 && wcscmp(arguments, L"--startup") == 0};
}

LSTATUS SaveShortcut(const std::filesystem::path& path) {
    Apartment apartment;
    if (FAILED(apartment.result) && apartment.result != RPC_E_CHANGED_MODE) return Win32Error(apartment.result);
    const auto executable = StartupExecutable();
    if (executable.empty()) return ERROR_BAD_PATHNAME;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return static_cast<LSTATUS>(error.value());
    IShellLinkW* rawLink = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&rawLink));
    if (FAILED(hr)) return Win32Error(hr);
    std::unique_ptr<IShellLinkW, ReleaseCom> link(rawLink);
    if (FAILED(hr = link->SetPath(executable.c_str())) ||
        FAILED(hr = link->SetWorkingDirectory(executable.parent_path().c_str())) ||
        FAILED(hr = link->SetArguments(L"--startup")) ||
        FAILED(hr = link->SetDescription(L"Layrix — запуск при входе в Windows"))) return Win32Error(hr);
    link->SetShowCmd(SW_SHOWNORMAL);
    IPersistFile* rawFile = nullptr;
    hr = link->QueryInterface(IID_PPV_ARGS(&rawFile));
    if (FAILED(hr)) return Win32Error(hr);
    std::unique_ptr<IPersistFile, ReleaseCom> file(rawFile);
    const auto temporary = path.wstring() + L".tmp";
    hr = file->Save(temporary.c_str(), TRUE);
    if (FAILED(hr)) { DeleteFileW(temporary.c_str()); return Win32Error(hr); }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto moveError = GetLastError();
        DeleteFileW(temporary.c_str());
        return static_cast<LSTATUS>(moveError);
    }
    return ERROR_SUCCESS;
}
}

StartupPaths UserStartupPaths() {
    StartupPaths paths;
    PWSTR folder = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_Startup, KF_FLAG_DEFAULT, nullptr, &folder);
    if (FAILED(hr)) { paths.error = Win32Error(hr); return paths; }
    paths.shortcut = std::filesystem::path(folder) / L"Layrix.lnk";
    CoTaskMemFree(folder);
    return paths;
}

std::filesystem::path StartupExecutable() {
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size()) return {};
    executable.resize(length);
    return executable;
}

std::wstring StartupCommand() { return L"\"" + StartupExecutable().wstring() + L"\" --startup"; }

StartupState ReadStartup(const StartupPaths& paths) {
    if (paths.error != ERROR_SUCCESS) return {false, paths.error};
    if (paths.shortcut.empty()) return {false, ERROR_BAD_PATHNAME};
    const auto shortcut = ReadShortcut(paths.shortcut);
    if (shortcut.enabled) return shortcut;
    const auto legacy = ReadLegacy(paths.legacyRunKey);
    if (legacy.enabled) return legacy;
    return {false, shortcut.error != ERROR_SUCCESS ? shortcut.error : legacy.error};
}

LSTATUS SetStartup(bool enabled, const StartupPaths& paths) {
    if (paths.error != ERROR_SUCCESS) return paths.error;
    if (paths.shortcut.empty()) return ERROR_BAD_PATHNAME;
    if (enabled) {
        const auto error = SaveShortcut(paths.shortcut);
        if (error != ERROR_SUCCESS) return error;
        // Only remove the v1.0 registration after the new shortcut is safely saved.
        return RemoveLegacy(paths.legacyRunKey);
    }
    const auto error = RemoveLegacy(paths.legacyRunKey);
    if (error != ERROR_SUCCESS) return error;
    if (!DeleteFileW(paths.shortcut.c_str())) {
        const auto deleteError = GetLastError();
        if (!Missing(deleteError)) return static_cast<LSTATUS>(deleteError);
    }
    return ERROR_SUCCESS;
}
}
