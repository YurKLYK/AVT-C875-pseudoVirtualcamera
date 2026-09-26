#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <cwchar>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool EqualsIgnoreCase(const wchar_t *a, const wchar_t *b) {
    return _wcsicmp(a, b) == 0;
}

std::vector<DWORD> FindTargets() {
    std::vector<DWORD> pids;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return pids;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (EqualsIgnoreCase(entry.szExeFile, L"RECentral.exe") ||
                EqualsIgnoreCase(entry.szExeFile, L"AVerRECentral.exe")) {
                pids.push_back(entry.th32ProcessID);
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pids;
}

bool Inject(DWORD pid, const std::wstring &dll_path) {
	HANDLE module_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (module_snapshot != INVALID_HANDLE_VALUE) {
		MODULEENTRY32W module{};
		module.dwSize = sizeof(module);
		if (Module32FirstW(module_snapshot, &module)) {
			do {
				if (_wcsicmp(module.szExePath, dll_path.c_str()) == 0) {
					CloseHandle(module_snapshot);
					std::wcout << L"[OK] PID " << pid << L": hook DLL already loaded\n";
					return true;
				}
			} while (Module32NextW(module_snapshot, &module));
		}
		CloseHandle(module_snapshot);
	}

    constexpr DWORD access = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                             PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;
    HANDLE process = OpenProcess(access, FALSE, pid);
    if (process == nullptr) {
        std::wcerr << L"[FAIL] PID " << pid << L": OpenProcess error "
                   << GetLastError() << L" (run as administrator if needed)\n";
        return false;
    }

    const SIZE_T bytes = (dll_path.size() + 1) * sizeof(wchar_t);
    void *remote_path = VirtualAllocEx(process, nullptr, bytes,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote_path == nullptr ||
        !WriteProcessMemory(process, remote_path, dll_path.c_str(), bytes, nullptr)) {
        std::wcerr << L"[FAIL] PID " << pid << L": unable to copy DLL path, error "
                   << GetLastError() << L"\n";
        if (remote_path != nullptr) VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    auto load_library = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(kernel32, "LoadLibraryW"));
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, load_library,
                                       remote_path, 0, nullptr);
    if (thread == nullptr) {
        std::wcerr << L"[FAIL] PID " << pid << L": CreateRemoteThread error "
                   << GetLastError() << L"\n";
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    WaitForSingleObject(thread, 10000);
    DWORD module = 0;
    GetExitCodeThread(thread, &module);

    CloseHandle(thread);
    VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
    CloseHandle(process);

    if (module == 0) {
        std::wcerr << L"[FAIL] PID " << pid << L": LoadLibraryW failed\n";
        return false;
    }

    std::wcout << L"[OK] PID " << pid << L": hook DLL loaded\n";
    return true;
}

}  // namespace

int wmain(int argc, wchar_t **argv) {
    wchar_t exe_path[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) == 0) return 1;

    std::wstring dll_path;
    if (argc >= 2) {
        wchar_t full_path[MAX_PATH]{};
        if (GetFullPathNameW(argv[1], MAX_PATH, full_path, nullptr) == 0) return 1;
        dll_path = full_path;
    } else {
        wchar_t *slash = std::wcsrchr(exe_path, L'\\');
        if (slash == nullptr) return 1;
        *(slash + 1) = L'\0';
        dll_path = std::wstring(exe_path) + L"recentral_share_hook.dll";
    }

    if (GetFileAttributesW(dll_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wcerr << L"Hook DLL not found: " << dll_path << L"\n";
        return 1;
    }

    const std::vector<DWORD> targets = FindTargets();
    if (targets.empty()) {
        std::wcerr << L"RECentral.exe / AVerRECentral.exe is not running.\n";
        return 2;
    }

    bool any_success = false;
    for (DWORD pid : targets) any_success = Inject(pid, dll_path) || any_success;

    if (any_success) {
        std::wcout << L"Start a new recording, then test the growing .ts with ffprobe.\n"
                   << L"Hook logs: %TEMP%\\recentral-share-hook-<pid>.jsonl\n";
        return 0;
    }
    return 3;
}
