#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <MinHook.h>

#include <cstdint>
#include <cstring>
#include <cwchar>

namespace {

using CreateFileWFn = HANDLE(WINAPI *)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
                                       DWORD, DWORD, HANDLE);
using CreateFileAFn = HANDLE(WINAPI *)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
                                       DWORD, DWORD, HANDLE);

CreateFileWFn g_create_file_w = nullptr;
CreateFileAFn g_create_file_a = nullptr;
HANDLE g_log = INVALID_HANDLE_VALUE;

bool IsTsWrite(LPCWSTR path, DWORD desired_access) {
    if (path == nullptr || (desired_access & (GENERIC_WRITE | FILE_APPEND_DATA)) == 0) {
        return false;
    }

    const wchar_t *dot = std::wcsrchr(path, L'.');
    return dot != nullptr && _wcsicmp(dot, L".ts") == 0;
}

bool IsTsWrite(LPCSTR path, DWORD desired_access) {
    if (path == nullptr || (desired_access & (GENERIC_WRITE | FILE_APPEND_DATA)) == 0) {
        return false;
    }

    const char *dot = std::strrchr(path, '.');
    return dot != nullptr && _stricmp(dot, ".ts") == 0;
}

void JsonEscapeAndAppend(char *dst, size_t dst_size, const wchar_t *src) {
    size_t used = 0;
    if (dst_size == 0) return;

    for (const wchar_t *p = src; p != nullptr && *p != L'\0' && used + 2 < dst_size; ++p) {
        wchar_t wc = *p;
        if (wc == L'\\' || wc == L'\"') {
            dst[used++] = '\\';
            dst[used++] = static_cast<char>(wc);
        } else if (wc >= 0x20 && wc <= 0x7e) {
            dst[used++] = static_cast<char>(wc);
        } else {
            dst[used++] = '?';
        }
    }
    dst[used] = '\0';
}

void WriteLog(const wchar_t *path, DWORD original_share, DWORD patched_share,
              HANDLE result, DWORD error) {
    if (g_log == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME st{};
    GetSystemTime(&st);

    char escaped_path[2048]{};
    JsonEscapeAndAppend(escaped_path, sizeof(escaped_path), path != nullptr ? path : L"");

    char line[3072]{};
    const int length = _snprintf_s(
        line, sizeof(line), _TRUNCATE,
        "{\"timestamp_utc\":\"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\","
        "\"event\":\"create_file\",\"pid\":%lu,\"path\":\"%s\","
        "\"original_share_mode\":%lu,\"patched_share_mode\":%lu,"
        "\"success\":%s,\"win32_error\":%lu}\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
        st.wMilliseconds, GetCurrentProcessId(), escaped_path, original_share,
        patched_share, result != INVALID_HANDLE_VALUE ? "true" : "false", error);

    if (length > 0) {
        DWORD written = 0;
        WriteFile(g_log, line, static_cast<DWORD>(length), &written, nullptr);
        FlushFileBuffers(g_log);
    }
}

HANDLE WINAPI HookCreateFileW(LPCWSTR path, DWORD desired_access, DWORD share_mode,
                              LPSECURITY_ATTRIBUTES security_attributes,
                              DWORD creation_disposition, DWORD flags_and_attributes,
                              HANDLE template_file) {
    const bool patch = IsTsWrite(path, desired_access);
    const DWORD patched_share = patch ? (share_mode | FILE_SHARE_READ) : share_mode;

    HANDLE result = g_create_file_w(path, desired_access, patched_share,
                                    security_attributes, creation_disposition,
                                    flags_and_attributes, template_file);
    const DWORD error = GetLastError();
    if (patch) WriteLog(path, share_mode, patched_share, result, error);
    SetLastError(error);
    return result;
}

HANDLE WINAPI HookCreateFileA(LPCSTR path, DWORD desired_access, DWORD share_mode,
                              LPSECURITY_ATTRIBUTES security_attributes,
                              DWORD creation_disposition, DWORD flags_and_attributes,
                              HANDLE template_file) {
    const bool patch = IsTsWrite(path, desired_access);
    const DWORD patched_share = patch ? (share_mode | FILE_SHARE_READ) : share_mode;

    HANDLE result = g_create_file_a(path, desired_access, patched_share,
                                    security_attributes, creation_disposition,
                                    flags_and_attributes, template_file);
    const DWORD error = GetLastError();

    if (patch) {
        wchar_t wide_path[1024]{};
        MultiByteToWideChar(CP_ACP, 0, path, -1, wide_path,
                            static_cast<int>(sizeof(wide_path) / sizeof(wide_path[0])));
        WriteLog(wide_path, share_mode, patched_share, result, error);
    }
    SetLastError(error);
    return result;
}

void OpenLog() {
    wchar_t temp_path[MAX_PATH]{};
    wchar_t log_path[MAX_PATH]{};
    if (GetTempPathW(static_cast<DWORD>(sizeof(temp_path) / sizeof(temp_path[0])), temp_path) == 0) return;

    _snwprintf_s(log_path, sizeof(log_path) / sizeof(log_path[0]), _TRUNCATE,
                 L"%srecentral-share-hook-%lu.jsonl", temp_path,
                 GetCurrentProcessId());

    g_log = CreateFileW(log_path, FILE_APPEND_DATA,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

DWORD WINAPI InstallHooks(LPVOID) {
    OpenLog();

    if (MH_Initialize() != MH_OK) return 1;
    if (MH_CreateHookApi(L"kernel32.dll", "CreateFileW",
                         reinterpret_cast<LPVOID>(&HookCreateFileW),
                         reinterpret_cast<LPVOID *>(&g_create_file_w)) != MH_OK) {
        return 2;
    }
    if (MH_CreateHookApi(L"kernel32.dll", "CreateFileA",
                         reinterpret_cast<LPVOID>(&HookCreateFileA),
                         reinterpret_cast<LPVOID *>(&g_create_file_a)) != MH_OK) {
        return 3;
    }
    return MH_EnableHook(MH_ALL_HOOKS) == MH_OK ? 0 : 4;
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(nullptr, 0, InstallHooks, nullptr, 0, nullptr);
        if (thread != nullptr) CloseHandle(thread);
    }
    return TRUE;
}
