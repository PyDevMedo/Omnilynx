#include "omnilynx_raii.hpp"
#include "omnilynx_hwprofile.hpp"
#include "omnilynx_net.hpp"
#include "omnilynx_external.hpp"

#include <tlhelp32.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using namespace omnilynx;

void PrintJsonError(const char* msg, DWORD err, const char* context = nullptr) {
    if (context) {
        AppendErrorLog(context, err, msg ? msg : "");
    }
    std::string logPath = WideToUtf8(GetLocalAppDataOmnilynxDir() + L"\\error.log");
    printf("{\"ok\":false,\"error\":\"%s\",\"win32Error\":%lu,\"win32Message\":\"%s\",\"logPath\":\"%s\"}\n",
           EscapeJsonUtf8(msg ? msg : "Unknown error").c_str(),
           static_cast<unsigned long>(err),
           EscapeJsonUtf8(FormatWin32ErrorUtf8(err)).c_str(),
           EscapeJsonUtf8(logPath).c_str());
}

std::vector<DWORD> FindProcessesByNameOrPid(const wchar_t* target) {
    std::vector<DWORD> pids;
    if (!target || *target == L'\0') return pids;

    wchar_t* endPtr = nullptr;
    unsigned long pidVal = wcstoul(target, &endPtr, 10);
    if (endPtr != target && *endPtr == L'\0' && pidVal > 0) {
        pids.push_back(static_cast<DWORD>(pidVal));
        return pids;
    }

    ScopedHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap) return pids;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap.get(), &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, target) == 0) {
                pids.push_back(pe.th32ProcessID);
            }
        } while (Process32NextW(snap.get(), &pe));
    }
    return pids;
}

HMODULE FindLoadedModule(DWORD pid, const wchar_t* dllNameOrPath) {
    if (!dllNameOrPath) return nullptr;
    const wchar_t* baseName = wcsrchr(dllNameOrPath, L'\\');
    baseName = baseName ? baseName + 1 : dllNameOrPath;

    ScopedHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
    if (!snap) return nullptr;

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    if (Module32FirstW(snap.get(), &me)) {
        do {
            if (_wcsicmp(me.szModule, baseName) == 0 || _wcsicmp(me.szExePath, dllNameOrPath) == 0) {
                return me.hModule;
            }
        } while (Module32NextW(snap.get(), &me));
    }
    return nullptr;
}

std::wstring ResolveFullPathW(const wchar_t* path) {
    DWORD needed = GetFullPathNameW(path, 0, nullptr, nullptr);
    if (needed == 0) return {};
    std::wstring out(needed, L'\0');
    DWORD written = GetFullPathNameW(path, needed, out.data(), nullptr);
    if (written == 0 || written >= needed) return {};
    out.resize(written);
    return out;
}

int CmdListProcesses() {
    ScopedHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap) {
        PrintJsonError("CreateToolhelp32Snapshot failed", GetLastError());
        return 1;
    }

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    printf("{\"ok\":true,\"processes\":[");
    bool first = true;

    if (Process32FirstW(snap.get(), &pe)) {
        do {
            std::string utf8Name = EscapeJsonUtf8(WideToUtf8(pe.szExeFile));
            if (!first) printf(",");
            first = false;
            printf("{\"pid\":%lu,\"name\":\"%s\"}",
                   static_cast<unsigned long>(pe.th32ProcessID),
                   utf8Name.c_str());
        } while (Process32NextW(snap.get(), &pe));
    }

    printf("]}\n");
    return 0;
}

int CmdInject(const wchar_t* target, const wchar_t* dllPath) {
    std::vector<DWORD> pids = FindProcessesByNameOrPid(target);
    if (pids.empty()) {
        PrintJsonError("Target process not found", 0);
        return 1;
    }

    std::wstring fullPath = ResolveFullPathW(dllPath);
    if (fullPath.empty()) {
        PrintJsonError("Invalid DLL path", GetLastError());
        return 1;
    }

    if (GetFileAttributesW(fullPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        PrintJsonError("DLL file does not exist", GetLastError());
        return 1;
    }

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    auto loadLibAddr = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        reinterpret_cast<void*>(GetProcAddress(hKernel32, "LoadLibraryW")));
    if (!loadLibAddr) {
        PrintJsonError("Failed to resolve LoadLibraryW", GetLastError());
        return 1;
    }

    DWORD lastErr = 0;
    const char* lastErrMsg = "OpenProcess failed";
    DWORD firstOkPid = 0;
    int injectedCount = 0;

    const size_t pathBytes = (fullPath.size() + 1) * sizeof(wchar_t);

    for (DWORD pid : pids) {
        if (FindLoadedModule(pid, fullPath.c_str()) != nullptr) {
            if (firstOkPid == 0) firstOkPid = pid;
            injectedCount++;
            continue;
        }

        ScopedHandle hProcess(OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
            FALSE,
            pid));
        if (!hProcess) {
            lastErr = GetLastError();
            lastErrMsg = "OpenProcess failed (try running as Administrator)";
            continue;
        }

        ScopedRemoteMem remoteMem(
            hProcess.get(),
            VirtualAllocEx(hProcess.get(), nullptr, pathBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!remoteMem) {
            lastErr = GetLastError();
            lastErrMsg = "VirtualAllocEx failed";
            continue;
        }

        if (!WriteProcessMemory(hProcess.get(), remoteMem.get(), fullPath.c_str(), pathBytes, nullptr)) {
            lastErr = GetLastError();
            lastErrMsg = "WriteProcessMemory failed";
            continue;
        }

        ScopedHandle hThread(CreateRemoteThread(hProcess.get(), nullptr, 0, loadLibAddr, remoteMem.get(), 0, nullptr));
        if (!hThread) {
            lastErr = GetLastError();
            lastErrMsg = "CreateRemoteThread failed";
            continue;
        }

        DWORD waitRes = WaitForSingleObject(hThread.get(), 10000);
        if (waitRes != WAIT_OBJECT_0) {
            remoteMem.release();
            lastErr = GetLastError();
            lastErrMsg = "Remote LoadLibraryW timed out";
            continue;
        }

        DWORD exitCode = 0;
        GetExitCodeThread(hThread.get(), &exitCode);
        if (exitCode == 0) {
            lastErr = 0;
            lastErrMsg = "Remote LoadLibraryW returned NULL";
            continue;
        }

        if (firstOkPid == 0) firstOkPid = pid;
        injectedCount++;
    }

    pids.clear();
    pids.shrink_to_fit();

    if (injectedCount == 0) {
        PrintJsonError(lastErrMsg, lastErr, ("Inject failed for target=" + WideToUtf8(target) + " dll=" + WideToUtf8(fullPath)).c_str());
        return 1;
    }

    printf("{\"ok\":true,\"pid\":%lu,\"count\":%d,\"injected\":true}\n",
           static_cast<unsigned long>(firstOkPid),
           injectedCount);
    return 0;
}

int CmdEject(const wchar_t* target, const wchar_t* dllName) {
    std::vector<DWORD> pids = FindProcessesByNameOrPid(target);
    if (pids.empty()) {
        PrintJsonError("Target process not found", 0);
        return 1;
    }

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    auto freeLibAddr = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        reinterpret_cast<void*>(GetProcAddress(hKernel32, "FreeLibrary")));
    if (!freeLibAddr) {
        PrintJsonError("Failed to resolve FreeLibrary", GetLastError());
        return 1;
    }

    int ejectedCount = 0;
    DWORD firstPid = pids[0];

    for (DWORD pid : pids) {
        HMODULE foundMod = FindLoadedModule(pid, dllName);
        if (!foundMod) continue;

        ScopedHandle hProcess(OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION,
            FALSE,
            pid));
        if (!hProcess) continue;

        ScopedHandle hThread(CreateRemoteThread(
            hProcess.get(), nullptr, 0, freeLibAddr, reinterpret_cast<LPVOID>(foundMod), 0, nullptr));
        if (hThread && WaitForSingleObject(hThread.get(), 10000) == WAIT_OBJECT_0) {
            DWORD exitCode = 0;
            if (GetExitCodeThread(hThread.get(), &exitCode) && exitCode != 0) {
                ejectedCount++;
            }
        }
    }

    pids.clear();
    pids.shrink_to_fit();

    if (ejectedCount == 0) {
        printf("{\"ok\":true,\"pid\":%lu,\"ejected\":false,\"reason\":\"not_loaded\"}\n",
               static_cast<unsigned long>(firstPid));
        return 0;
    }

    printf("{\"ok\":true,\"pid\":%lu,\"count\":%d,\"ejected\":true}\n",
           static_cast<unsigned long>(firstPid),
           ejectedCount);
    return 0;
}

int CmdSysInfo() {
    HardwareProfile hw = DetectHardwareProfile();
    std::wstring dataDir = GetLocalAppDataOmnilynxDir();
    bool elevated = IsProcessElevated();

    printf("{\"ok\":true,\"logicalCores\":%lu,\"totalRamMB\":%llu,\"availRamMB\":%llu,"
           "\"gpuVramMB\":%llu,\"gpuName\":\"%s\",\"osMajor\":%lu,\"osMinor\":%lu,"
           "\"osBuild\":%lu,\"isWindows11\":%s,\"tier\":\"%s\","
           "\"recommendedThreads\":%lu,\"ioBufferBytes\":%zu,\"backgroundPollMs\":%lu,"
           "\"elevated\":%s,\"dataDir\":\"%s\"}\n",
           static_cast<unsigned long>(hw.logicalCores),
           static_cast<unsigned long long>(hw.totalRamMB),
           static_cast<unsigned long long>(hw.availRamMB),
           static_cast<unsigned long long>(hw.gpuVramMB),
           EscapeJsonUtf8(WideToUtf8(hw.gpuName)).c_str(),
           static_cast<unsigned long>(hw.osMajor),
           static_cast<unsigned long>(hw.osMinor),
           static_cast<unsigned long>(hw.osBuild),
           hw.isWindows11 ? "true" : "false",
           HardwareTierName(hw.tier),
           static_cast<unsigned long>(hw.recommendedThreads),
           hw.ioBufferBytes,
           static_cast<unsigned long>(hw.backgroundPollMs),
           elevated ? "true" : "false",
           EscapeJsonUtf8(WideToUtf8(dataDir)).c_str());
    return 0;
}

int CmdFetch(const wchar_t* modFileName) {
    std::vector<uint8_t> bytes;
    std::string sha256;
    std::string error;
    if (!FetchModSourceWithCache(modFileName, bytes, sha256, error)) {
        PrintJsonError(error.c_str(), GetLastError());
        return 1;
    }
    const size_t size = bytes.size();
    bytes.clear();
    bytes.shrink_to_fit();

    printf("{\"ok\":true,\"file\":\"%s\",\"bytes\":%zu,\"sha256\":\"%s\"}\n",
           EscapeJsonUtf8(WideToUtf8(modFileName)).c_str(),
           size,
           sha256.c_str());
    return 0;
}

int CmdInstall(const wchar_t* stagedTemp, const wchar_t* targetPath, const wchar_t* backupPath) {
    if (PathRequiresElevation(targetPath) && !IsProcessElevated()) {
        PrintJsonError("UAC elevation required for protected directory", ERROR_ACCESS_DENIED);
        return 1;
    }
    std::wstring backup = backupPath ? backupPath : L"";
    if (!AtomicReplaceFileW(stagedTemp, targetPath, backup)) {
        PrintJsonError("Atomic file installation failed", GetLastError());
        return 1;
    }
    printf("{\"ok\":true,\"installed\":true}\n");
    return 0;
}

int CmdRollback(const wchar_t* backupPath, const wchar_t* targetPath) {
    if (PathRequiresElevation(targetPath) && !IsProcessElevated()) {
        PrintJsonError("UAC elevation required for protected directory", ERROR_ACCESS_DENIED);
        return 1;
    }
    if (!RestoreBackupFileW(backupPath, targetPath)) {
        PrintJsonError("Rollback failed", GetLastError());
        return 1;
    }
    printf("{\"ok\":true,\"restored\":true}\n");
    return 0;
}

int CmdApply(const wchar_t* modId, const wchar_t* targetExe, const wchar_t* stateStr, const wchar_t* paramStr) {
    const bool enable = (_wcsicmp(stateStr, L"on") == 0 || _wcsicmp(stateStr, L"enable") == 0 || wcscmp(stateStr, L"1") == 0);
    const int paramVal = paramStr ? static_cast<int>(wcstol(paramStr, nullptr, 10)) : 0;
    ExternalApplyResult r = ApplyExternalMod(modId, targetExe, enable, paramVal);

    // If enabling a per-window mod and target has no visible windows yet, launch it via CreateProcessW and apply
    if (!r.ok) {
        PrintJsonError(r.error.c_str(), r.win32Error, ("CmdApply " + WideToUtf8(modId)).c_str());
        return 1;
    }

    printf("{\"ok\":true,\"external\":true,\"affectedWindows\":%d}\n", r.affectedWindows);
    return 0;
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    (void)EnableSeDebugPrivilege();
    if (argc < 2) {
        printf("{\"ok\":false,\"error\":\"Usage: wendo_injector <list|inject|eject|apply|sysinfo|fetch|install|rollback> ...\"}\n");
        return 1;
    }

    if (_wcsicmp(argv[1], L"apply") == 0 && argc >= 5) {
        return CmdApply(argv[2], argv[3], argv[4], argc >= 6 ? argv[5] : nullptr);
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
    if (_wcsicmp(argv[1], L"sysinfo") == 0) {
        return CmdSysInfo();
    }
    if (_wcsicmp(argv[1], L"fetch") == 0 && argc >= 3) {
        return CmdFetch(argv[2]);
    }
    if (_wcsicmp(argv[1], L"install") == 0 && argc >= 4) {
        return CmdInstall(argv[2], argv[3], argc >= 5 ? argv[4] : nullptr);
    }
    if (_wcsicmp(argv[1], L"rollback") == 0 && argc >= 4) {
        return CmdRollback(argv[2], argv[3]);
    }

    printf("{\"ok\":false,\"error\":\"Invalid arguments\"}\n");
    return 1;
}
