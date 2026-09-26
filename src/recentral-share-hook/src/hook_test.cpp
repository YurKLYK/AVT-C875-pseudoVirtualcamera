#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>

int wmain() {
    wchar_t exe_path[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) == 0) return 1;
    wchar_t *slash = wcsrchr(exe_path, L'\\');
    if (slash == nullptr) return 1;
    *(slash + 1) = L'\0';

    wchar_t dll_path[MAX_PATH]{};
    _snwprintf_s(dll_path, MAX_PATH, _TRUNCATE,
                 L"%srecentral_share_hook.dll", exe_path);
    if (LoadLibraryW(dll_path) == nullptr) {
        std::wcerr << L"LoadLibrary failed: " << GetLastError() << L"\n";
        return 2;
    }

    // The hook is installed by a worker thread created from DllMain.
    Sleep(500);

    wchar_t temp_dir[MAX_PATH]{};
    wchar_t test_path[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp_dir);
    _snwprintf_s(test_path, MAX_PATH, _TRUNCATE,
                 L"%srecentral-share-hook-test-%lu.ts", temp_dir,
                 GetCurrentProcessId());

    HANDLE writer = CreateFileW(test_path, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (writer == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Writer open failed: " << GetLastError() << L"\n";
        return 3;
    }

    const char payload[] = "hook-test";
    DWORD written = 0;
    WriteFile(writer, payload, sizeof(payload), &written, nullptr);
    FlushFileBuffers(writer);

    HANDLE reader = CreateFileW(test_path, GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    const DWORD reader_error = GetLastError();

    if (reader != INVALID_HANDLE_VALUE) CloseHandle(reader);
    CloseHandle(writer);
    DeleteFileW(test_path);

    if (reader == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Concurrent read failed: " << reader_error << L"\n";
        return 4;
    }

    std::wcout << L"PASS: a .ts writer opened with share=0 was readable concurrently.\n";
    return 0;
}

