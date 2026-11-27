#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

static void PrintJsonError(const char* msg, DWORD err) {
    printf("{\"ok\":false,\"error\":\"%s\",\"win32Error\":%lu}\n", msg, (unsigned long)err);
}

static int FindProcessesByNameOrPid(const wchar_t* target, DWORD* outPids, int maxPids) {
    wchar_t* endPtr = nullptr;
    unsigned long pidVal = wcstoul(target, &endPtr, 10);
    if (endPtr != target && *endPtr == L'\0' && pidVal > 0) {
        outPids[0] = (DWORD)pidVal;
        return 1;
    }

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    int count = 0;

    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, target) == 0) {
                if (count < maxPids) {
                    outPids[count++] = pe.th32ProcessID;
                }
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return count;
}

static HMODULE FindLoadedModule(DWORD pid, const wchar_t* dllNameOrPath) {
    const wchar_t* baseName = wcsrchr(dllNameOrPath, L'\\');
    baseName = baseName ? baseName + 1 : dllNameOrPath;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return nullptr;

    MODULEENTRY32W me;
    me.dwSize = sizeof(me);
    HMODULE foundMod = nullptr;

    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, baseName) == 0 || _wcsicmp(me.szExePath, dllNameOrPath) == 0) {
                foundMod = me.hModule;
                break;
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return foundMod;
}

static int CmdListProcesses() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        PrintJsonError("CreateToolhelp32Snapshot failed", GetLastError());
        return 1;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    printf("{\"ok\":true,\"processes\":[");
    bool first = true;

    if (Process32FirstW(snap, &pe)) {
        do {
            char utf8Name[MAX_PATH * 3];
            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, utf8Name, sizeof(utf8Name), nullptr, nullptr);
            if (!first) printf(",");
            first = false;
            printf("{\"pid\":%lu,\"name\":\"%s\"}", (unsigned long)pe.th32ProcessID, utf8Name);
        } while (Process32NextW(snap, &pe));
    }

    printf("]}\n");
    CloseHandle(snap);
    return 0;
}

static int CmdInject(const wchar_t* target, const wchar_t* dllPath) {
    DWORD pids[64];
    int pidCount = FindProcessesByNameOrPid(target, pids, 64);
    if (pidCount == 0) {
        PrintJsonError("Target process not found", 0);
        return 1;
    }

    wchar_t fullPath[MAX_PATH];
    if (!GetFullPathNameW(dllPath, MAX_PATH, fullPath, nullptr)) {
        PrintJsonError("Invalid DLL path", GetLastError());
        return 1;
    }

    if (GetFileAttributesW(fullPath) == INVALID_FILE_ATTRIBUTES) {
        PrintJsonError("DLL file does not exist", GetLastError());
        return 1;
    }

    DWORD lastErr = 0;
    const char* lastErrMsg = "OpenProcess failed";
    DWORD firstOkPid = 0;
    int injectedCount = 0;

    for (int i = 0; i < pidCount; i++) {
        DWORD pid = pids[i];
        if (FindLoadedModule(pid, fullPath) != nullptr) {
            if (firstOkPid == 0) firstOkPid = pid;
            injectedCount++;
            continue;
        }

        HANDLE hProcess = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
            FALSE,
            pid
        );
        if (!hProcess) {
            lastErr = GetLastError();
            lastErrMsg = "OpenProcess failed (try running as Administrator)";
            continue;
        }

        size_t pathBytes = (wcslen(fullPath) + 1) * sizeof(wchar_t);
        LPVOID remoteMem = VirtualAllocEx(hProcess, nullptr, pathBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remoteMem) {
            lastErr = GetLastError();
            lastErrMsg = "VirtualAllocEx failed";
            CloseHandle(hProcess);
            continue;
        }

        if (!WriteProcessMemory(hProcess, remoteMem, fullPath, pathBytes, nullptr)) {
            lastErr = GetLastError();
            lastErrMsg = "WriteProcessMemory failed";
            VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            continue;
        }

        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        LPTHREAD_START_ROUTINE loadLibAddr = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW");

        HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, loadLibAddr, remoteMem, 0, nullptr);
        if (!hThread) {
            lastErr = GetLastError();
            lastErrMsg = "CreateRemoteThread failed";
            VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            continue;
        }

        WaitForSingleObject(hThread, 10000);
        DWORD exitCode = 0;
        GetExitCodeThread(hThread, &exitCode);

        CloseHandle(hThread);
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);

        if (exitCode == 0) {
            lastErr = 0;
            lastErrMsg = "Remote LoadLibraryW returned NULL";
            continue;
        }

        if (firstOkPid == 0) firstOkPid = pid;
        injectedCount++;
    }

    if (injectedCount == 0) {
        PrintJsonError(lastErrMsg, lastErr);
        return 1;
    }

    printf("{\"ok\":true,\"pid\":%lu,\"count\":%d,\"injected\":true}\n", (unsigned long)firstOkPid, injectedCount);
    return 0;
}

static int CmdEject(const wchar_t* target, const wchar_t* dllName) {
    DWORD pids[64];
    int pidCount = FindProcessesByNameOrPid(target, pids, 64);
    if (pidCount == 0) {
        PrintJsonError("Target process not found", 0);
        return 1;
    }

    int ejectedCount = 0;
    DWORD firstPid = pids[0];

    for (int i = 0; i < pidCount; i++) {
        DWORD pid = pids[i];
        HMODULE foundMod = FindLoadedModule(pid, dllName);
        if (!foundMod) continue;

        HANDLE hProcess = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION,
            FALSE,
            pid
        );
        if (!hProcess) continue;

        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        LPTHREAD_START_ROUTINE freeLibAddr = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "FreeLibrary");
        HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, freeLibAddr, (LPVOID)foundMod, 0, nullptr);
        if (hThread) {
            WaitForSingleObject(hThread, 10000);
            CloseHandle(hThread);
            ejectedCount++;
        }
        CloseHandle(hProcess);
    }

    if (ejectedCount == 0) {
        printf("{\"ok\":true,\"pid\":%lu,\"ejected\":false,\"reason\":\"not_loaded\"}\n", (unsigned long)firstPid);
        return 0;
    }

    printf("{\"ok\":true,\"pid\":%lu,\"count\":%d,\"ejected\":true}\n", (unsigned long)firstPid, ejectedCount);
    return 0;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        printf("{\"ok\":false,\"error\":\"Usage: wendo_injector <list|inject|eject> ...\"}\n");
        return 1;
    }

    if (_wcsicmp(argv[1], L"list") == 0) {
        return CmdListProcesses();
    }
    if (_wcsicmp(argv[1], L"inject") == 0 && argc >= 4) {
        return CmdInject(argv[2], argv[3]);
    }
    if (_wcsicmp(argv[1], L"eject") == 0 && argc >= 4) {
        return CmdEject(argv[2], argv[3]);
    }

    printf("{\"ok\":false,\"error\":\"Invalid arguments\"}\n");
    return 1;
}
