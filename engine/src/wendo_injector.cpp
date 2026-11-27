#include "omnilynx_raii.hpp"
#include "omnilynx_hwprofile.hpp"
#include "omnilynx_net.hpp"
#include "omnilynx_external.hpp"

#include <tlhelp32.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <winternl.h>

// ── NtCreateThreadEx & RtlCreateUserThread typedefs (CFG bypass for explorer.exe) ──
// NtCreateThreadEx is an undocumented ntdll export that skips CFG validation on
// the thread start-routine, unlike CreateRemoteThread which enforces CFG on Win11.
typedef NTSTATUS (NTAPI *PFN_NtCreateThreadEx)(
    PHANDLE             ThreadHandle,
    ACCESS_MASK         DesiredAccess,
    LPVOID              ObjectAttributes,
    HANDLE              ProcessHandle,
    LPTHREAD_START_ROUTINE StartRoutine,
    LPVOID              Argument,
    ULONG               CreateFlags,       // 0x1 = CREATE_SUSPENDED
    SIZE_T              ZeroBits,
    SIZE_T              StackSize,
    SIZE_T              MaximumStackSize,
    LPVOID              AttributeList
);

typedef NTSTATUS (NTAPI *PFN_RtlCreateUserThread)(
    HANDLE              ProcessHandle,
    LPSECURITY_ATTRIBUTES SecurityDescriptor,
    BOOL                CreateSuspended,
    ULONG               StackZeroBits,
    SIZE_T*             StackReserve,
    SIZE_T*             StackCommit,
    LPTHREAD_START_ROUTINE StartAddress,
    LPVOID              Parameter,
    PHANDLE             ThreadHandle,
    LPVOID              ClientId        // PCLIENT_ID, ignored
);

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

DWORD FindGuiThreadForPid(DWORD targetPid) {
    struct Ctx {
        DWORD pid;
        DWORD tid;
    } ctx{ targetPid, 0 };

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* c = reinterpret_cast<Ctx*>(lParam);
        DWORD winPid = 0;
        DWORD winTid = GetWindowThreadProcessId(hwnd, &winPid);
        if (winPid == c->pid && winTid != 0) {
            c->tid = winTid;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    return ctx.tid;
}

// ── NtCreateThreadEx injection (primary CFG bypass) ────────────────────────────
// Windows 11 explorer.exe has CFG enabled (Policy 7 = 0x1). CreateRemoteThread
// passes LoadLibraryW as start routine → CFG rejects it → thread silently exits.
// NtCreateThreadEx bypasses the CFG check on the start-routine call site because
// the kernel validates only the thread's own call targets, not the initial RIP.
// This is the same approach used by Windhawk's injector.
bool InjectViaNtCreateThreadEx(HANDLE hProcess, DWORD pid, LPTHREAD_START_ROUTINE loadLibAddr,
                                LPVOID remoteMem, const std::wstring& fullPath) {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return false;
    auto pfnNtCreateThreadEx = reinterpret_cast<PFN_NtCreateThreadEx>(
        reinterpret_cast<void*>(GetProcAddress(hNtdll, "NtCreateThreadEx")));
    if (!pfnNtCreateThreadEx) {
        TraceRuntimeLog("Injector", "NtCreateThreadEx export not found in ntdll");
        return false;
    }

    HANDLE hThread = nullptr;
    // CreateFlags = 0 → run immediately (no suspend); DesiredAccess = THREAD_ALL_ACCESS
    NTSTATUS status = pfnNtCreateThreadEx(
        &hThread,
        THREAD_ALL_ACCESS,
        nullptr,
        hProcess,
        loadLibAddr,
        remoteMem,
        0,          // CreateFlags = 0 (not suspended)
        0, 0, 0,
        nullptr);

    if (status != 0 || !hThread) {
        TraceRuntimeLog("Injector", "NtCreateThreadEx FAILED pid=" + std::to_string(pid)
            + " status=0x" + [&]{ char buf[16]; snprintf(buf,sizeof(buf),"%08lX",(unsigned long)status); return std::string(buf); }());
        if (hThread) CloseHandle(hThread);
        return false;
    }

    TraceRuntimeLog("Injector", "NtCreateThreadEx OK pid=" + std::to_string(pid) + " threadHandle=" + std::to_string(reinterpret_cast<uintptr_t>(hThread)));

    // Wait up to 5s for LoadLibraryW to complete
    DWORD waitRes = WaitForSingleObject(hThread, 5000);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    bool loaded = (FindLoadedModule(pid, fullPath.c_str()) != nullptr);
    TraceRuntimeLog("Injector", "NtCreateThreadEx pid=" + std::to_string(pid)
        + " waitRes=" + std::to_string(waitRes)
        + " exitCode=" + std::to_string(exitCode)
        + " loaded=" + (loaded ? "YES" : "NO"));
    return loaded;
}

// ── RtlCreateUserThread injection (secondary CFG bypass) ────────────────────
bool InjectViaRtlCreateUserThread(HANDLE hProcess, DWORD pid, LPTHREAD_START_ROUTINE loadLibAddr,
                                   LPVOID remoteMem, const std::wstring& fullPath) {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return false;
    auto pfnRtlCreateUserThread = reinterpret_cast<PFN_RtlCreateUserThread>(
        reinterpret_cast<void*>(GetProcAddress(hNtdll, "RtlCreateUserThread")));
    if (!pfnRtlCreateUserThread) {
        TraceRuntimeLog("Injector", "RtlCreateUserThread export not found in ntdll");
        return false;
    }

    HANDLE hThread = nullptr;
    NTSTATUS status = pfnRtlCreateUserThread(
        hProcess,
        nullptr,    // SecurityDescriptor
        FALSE,      // not suspended
        0,
        nullptr, nullptr,
        loadLibAddr,
        remoteMem,
        &hThread,
        nullptr);

    if (status != 0 || !hThread) {
        TraceRuntimeLog("Injector", "RtlCreateUserThread FAILED pid=" + std::to_string(pid)
            + " status=0x" + [&]{ char buf[16]; snprintf(buf,sizeof(buf),"%08lX",(unsigned long)status); return std::string(buf); }());
        if (hThread) CloseHandle(hThread);
        return false;
    }

    TraceRuntimeLog("Injector", "RtlCreateUserThread OK pid=" + std::to_string(pid));
    DWORD waitRes = WaitForSingleObject(hThread, 5000);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    bool loaded = (FindLoadedModule(pid, fullPath.c_str()) != nullptr);
    TraceRuntimeLog("Injector", "RtlCreateUserThread pid=" + std::to_string(pid)
        + " waitRes=" + std::to_string(waitRes)
        + " exitCode=" + std::to_string(exitCode)
        + " loaded=" + (loaded ? "YES" : "NO"));
    return loaded;
}

bool InjectViaSetWindowsHook(DWORD pid, const std::wstring& fullPath) {
    DWORD uiTid = FindGuiThreadForPid(pid);
    if (uiTid == 0) {
        TraceRuntimeLog("Injector", "SetWindowsHook fallback: no GUI thread found for pid=" + std::to_string(pid));
        return false;
    }

    SetEnvironmentVariableW(L"OMNILYNX_INJECTOR_NO_INIT", L"1");
    ScopedLibrary localMod(LoadLibraryW(fullPath.c_str()));
    SetEnvironmentVariableW(L"OMNILYNX_INJECTOR_NO_INIT", nullptr);
    if (!localMod) {
        TraceRuntimeLog("Injector", "SetWindowsHook fallback: local LoadLibraryW failed err=" + FormatWin32ErrorUtf8(GetLastError()));
        return false;
    }

    auto hookProc = reinterpret_cast<HOOKPROC>(
        reinterpret_cast<void*>(GetProcAddress(localMod.get(), "OmnilynxGetMsgProc")));
    if (!hookProc) {
        hookProc = reinterpret_cast<HOOKPROC>(
            reinterpret_cast<void*>(GetProcAddress(localMod.get(), "_OmnilynxGetMsgProc@12")));
    }
    if (!hookProc) {
        TraceRuntimeLog("Injector", "SetWindowsHook fallback: OmnilynxGetMsgProc export not found");
        return false;
    }

    HHOOK hHook = SetWindowsHookExW(WH_GETMESSAGE, hookProc, localMod.get(), uiTid);
    if (!hHook) {
        TraceRuntimeLog("Injector", "SetWindowsHookExW failed err=" + FormatWin32ErrorUtf8(GetLastError()));
        return false;
    }

    PostThreadMessageW(uiTid, WM_NULL, 0, 0);
    bool loaded = false;
    for (int i = 0; i < 40; ++i) {
        Sleep(50);
        if (FindLoadedModule(pid, fullPath.c_str()) != nullptr) {
            loaded = true;
            break;
        }
    }
    UnhookWindowsHookEx(hHook);
    TraceRuntimeLog("Injector", std::string("SetWindowsHookExW pid=") + std::to_string(pid) + " tid=" + std::to_string(uiTid) + " loaded=" + (loaded ? "YES" : "NO"));
    return loaded;
}

bool InjectViaQueueUserAPC(DWORD pid, LPTHREAD_START_ROUTINE loadLibAddr, LPVOID remoteMem, const std::wstring& fullPath) {
    ScopedHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0));
    if (!snap) return false;

    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    int queued = 0;
    if (Thread32First(snap.get(), &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                ScopedHandle hThr(OpenThread(THREAD_SET_CONTEXT, FALSE, te.th32ThreadID));
                if (hThr) {
                    if (QueueUserAPC(reinterpret_cast<PAPCFUNC>(reinterpret_cast<void*>(loadLibAddr)), hThr.get(), reinterpret_cast<ULONG_PTR>(remoteMem))) {
                        queued++;
                    }
                }
            }
        } while (Thread32Next(snap.get(), &te));
    }

    if (queued == 0) {
        TraceRuntimeLog("Injector", "QueueUserAPC: 0 threads queued for pid=" + std::to_string(pid));
        return false;
    }

    bool loaded = false;
    for (int i = 0; i < 40; ++i) {
        Sleep(50);
        if (FindLoadedModule(pid, fullPath.c_str()) != nullptr) {
            loaded = true;
            break;
        }
    }
    TraceRuntimeLog("Injector", "QueueUserAPC pid=" + std::to_string(pid) + " queued=" + std::to_string(queued) + " loaded=" + (loaded ? "YES" : "NO"));
    return loaded;
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

#define OMNILYNX_IPC_MAGIC 0x4F4D4E49u

struct OmnilynxInitHandshake {
    DWORD magic;
    DWORD pid;
    BOOL completed;
    BOOL modInitOk;
    BOOL hooksOk;
    DWORD hooksCreated;
    DWORD hooksFailed;
    DWORD symbolsMissing;
    DWORD win32Error;
    char errorMsg[256];
};

std::wstring ExtractModIdFromPath(const std::wstring& fullPath) {
    const wchar_t* base = wcsrchr(fullPath.c_str(), L'\\');
    std::wstring name = base ? (base + 1) : fullPath;
    if (name.size() > 4 && _wcsicmp(name.c_str() + name.size() - 4, L".dll") == 0) {
        name.resize(name.size() - 4);
    }
    // Strip _<8-hex>_<digits> version suffix if present
    size_t lastUs = name.rfind(L'_');
    if (lastUs != std::wstring::npos && lastUs > 0) {
        size_t prevUs = name.rfind(L'_', lastUs - 1);
        if (prevUs != std::wstring::npos && (lastUs - prevUs - 1) == 8) {
            name.resize(prevUs);
        }
    }
    return name;
}

bool IsTarget32BitWow64(HANDLE hProcess) {
    BOOL isWow64 = FALSE;
    if (IsWow64Process(hProcess, &isWow64) && isWow64) {
        return true;
    }
    return false;
}

struct IpcHandshakeHandles {
    ScopedHandle hMap;
    ScopedHandle hDone;
    OmnilynxInitHandshake* view = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    ~IpcHandshakeHandles() {
        if (view) UnmapViewOfFile(view);
        if (sd) LocalFree(sd);
    }
};

bool CreateIpcHandshake(DWORD pid, const std::wstring& modId, IpcHandshakeHandles& out) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:(A;;GA;;;WD)(A;;GA;;;AC)", SDDL_REVISION_1, &out.sd, nullptr);
    sa.lpSecurityDescriptor = out.sd;

    std::wstring mapName = L"Local\\Omnilynx_IPC_" + std::to_wstring(pid) + L"_" + modId;
    std::wstring evtName = L"Local\\Omnilynx_IPC_Done_" + std::to_wstring(pid) + L"_" + modId;

    out.hDone.reset(CreateEventW(out.sd ? &sa : nullptr, TRUE, FALSE, evtName.c_str()));
    out.hMap.reset(CreateFileMappingW(
        INVALID_HANDLE_VALUE, out.sd ? &sa : nullptr, PAGE_READWRITE, 0, sizeof(OmnilynxInitHandshake), mapName.c_str()));
    if (!out.hMap || !out.hDone) return false;
    out.view = static_cast<OmnilynxInitHandshake*>(
        MapViewOfFile(out.hMap.get(), FILE_MAP_ALL_ACCESS, 0, 0, sizeof(OmnilynxInitHandshake)));
    if (!out.view) return false;
    memset(out.view, 0, sizeof(OmnilynxInitHandshake));
    return true;
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
    int verifiedCount = 0;
    DWORD totalHooksCreated = 0;
    std::string ipcErrorDetail;

    const size_t pathBytes = (fullPath.size() + 1) * sizeof(wchar_t);
    const std::wstring modId = ExtractModIdFromPath(fullPath);
    GrantAppPackagesReadExecute(fullPath);
    TraceRuntimeLog("Injector", "CmdInject start target=" + WideToUtf8(target) + " modId=" + WideToUtf8(modId) + " dll=" + WideToUtf8(fullPath) + " matchedPids=" + std::to_string(pids.size()));

    for (DWORD pid : pids) {
        if (FindLoadedModule(pid, fullPath.c_str()) != nullptr) {
            TraceRuntimeLog("Injector", "PID " + std::to_string(pid) + " already has DLL loaded");
            if (firstOkPid == 0) firstOkPid = pid;
            injectedCount++;
            verifiedCount++;
            continue;
        }

        ScopedHandle hProcess(OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
            FALSE,
            pid));
        if (!hProcess) {
            lastErr = GetLastError();
            lastErrMsg = (lastErr == ERROR_ACCESS_DENIED)
                ? "Protected Process (PPL) or elevated target - OpenProcess denied"
                : "OpenProcess failed (try running as Administrator)";
            TraceRuntimeLog("Injector", "OpenProcess FAILED pid=" + std::to_string(pid) + " err=" + FormatWin32ErrorUtf8(lastErr));
            continue;
        }
        TraceRuntimeLog("Injector", "OpenProcess OK pid=" + std::to_string(pid));

        if (IsTarget32BitWow64(hProcess.get())) {
            lastErr = ERROR_BAD_EXE_FORMAT;
            lastErrMsg = "Architecture mismatch: 32-bit (WOW64) target cannot load 64-bit mod DLL";
            TraceRuntimeLog("Injector", "Skipped 32-bit WOW64 pid=" + std::to_string(pid));
            continue;
        }

        IpcHandshakeHandles ipc;
        bool hasIpc = CreateIpcHandshake(pid, modId, ipc);

        ScopedRemoteMem remoteMem(
            hProcess.get(),
            VirtualAllocEx(hProcess.get(), nullptr, pathBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!remoteMem) {
            lastErr = GetLastError();
            lastErrMsg = "VirtualAllocEx failed";
            TraceRuntimeLog("Injector", "VirtualAllocEx FAILED pid=" + std::to_string(pid) + " err=" + FormatWin32ErrorUtf8(lastErr));
            continue;
        }

        SIZE_T bytesWritten = 0;
        if (!WriteProcessMemory(hProcess.get(), remoteMem.get(), fullPath.c_str(), pathBytes, &bytesWritten)) {
            lastErr = GetLastError();
            lastErrMsg = "WriteProcessMemory failed";
            TraceRuntimeLog("Injector", "WriteProcessMemory FAILED pid=" + std::to_string(pid) + " err=" + FormatWin32ErrorUtf8(lastErr));
            continue;
        }
        TraceRuntimeLog("Injector", "VirtualAllocEx=0x" + std::to_string(reinterpret_cast<uintptr_t>(remoteMem.get())) + " WriteProcessMemory bytes=" + std::to_string(bytesWritten));

        // ── Injection attempt order (most → least CFG-safe) ───────────────────────
        // 1. NtCreateThreadEx  → bypasses CFG on explorer.exe / Win11 protected procs
        // 2. RtlCreateUserThread → secondary ntdll bypass
        // 3. CreateRemoteThread  → standard, fails on CFG processes
        // 4. SetWindowsHookExW   → requires GUI thread owned by pid
        // 5. QueueUserAPC        → requires alertable thread

        bool thisInjected = false;

        // --- Attempt 1: NtCreateThreadEx (CFG bypass) ---
        TraceRuntimeLog("Injector", "Trying NtCreateThreadEx for pid=" + std::to_string(pid));
        if (InjectViaNtCreateThreadEx(hProcess.get(), pid, loadLibAddr, remoteMem.get(), fullPath)) {
            TraceRuntimeLog("Injector", "NtCreateThreadEx SUCCESS pid=" + std::to_string(pid));
            thisInjected = true;
        }

        if (!thisInjected) {
            // --- Attempt 2: RtlCreateUserThread ---
            TraceRuntimeLog("Injector", "NtCreateThreadEx failed, trying RtlCreateUserThread pid=" + std::to_string(pid));
            if (InjectViaRtlCreateUserThread(hProcess.get(), pid, loadLibAddr, remoteMem.get(), fullPath)) {
                TraceRuntimeLog("Injector", "RtlCreateUserThread SUCCESS pid=" + std::to_string(pid));
                thisInjected = true;
            }
        }

        if (!thisInjected) {
            // --- Attempt 3: Classic CreateRemoteThread ---
            TraceRuntimeLog("Injector", "NtCreateThreadEx+Rtl failed, trying CreateRemoteThread pid=" + std::to_string(pid));
            ScopedHandle hThread(CreateRemoteThread(hProcess.get(), nullptr, 0, loadLibAddr, remoteMem.get(), 0, nullptr));
            if (!hThread) {
                lastErr = GetLastError();
                lastErrMsg = "CreateRemoteThread failed";
                TraceRuntimeLog("Injector", "CreateRemoteThread FAILED pid=" + std::to_string(pid) + " err=" + FormatWin32ErrorUtf8(lastErr));
            } else {
                DWORD waitRes = WaitForSingleObject(hThread.get(), 4000);
                DWORD exitCode = 0;
                GetExitCodeThread(hThread.get(), &exitCode);
                HMODULE verifiedMod = FindLoadedModule(pid, fullPath.c_str());
                TraceRuntimeLog("Injector", "CreateRemoteThread pid=" + std::to_string(pid)
                    + " waitRes=" + std::to_string(waitRes)
                    + " exitCode=" + std::to_string(exitCode)
                    + " loadedModule=" + (verifiedMod ? "YES" : "NO"));
                if (waitRes == WAIT_OBJECT_0 && (exitCode != 0 || verifiedMod)) {
                    thisInjected = true;
                } else if (waitRes != WAIT_OBJECT_0) {
                    // Timed out → try hook methods
                    LPVOID rawRemoteMem = remoteMem.get();
                    TraceRuntimeLog("Injector", "CreateRemoteThread TIMEOUT pid=" + std::to_string(pid) + " -> trying SetWindowsHookExW / QueueUserAPC");
                    if (InjectViaSetWindowsHook(pid, fullPath) ||
                        InjectViaQueueUserAPC(pid, loadLibAddr, rawRemoteMem, fullPath)) {
                        thisInjected = true;
                    }
                } else {
                    lastErr = 0;
                    lastErrMsg = "Remote LoadLibraryW returned NULL";
                }
            }
        }

        if (!thisInjected) {
            // --- Attempt 4: SetWindowsHookExW / QueueUserAPC directly ---
            TraceRuntimeLog("Injector", "All thread methods failed, trying SetWindowsHook/APC pid=" + std::to_string(pid));
            LPVOID rawRemoteMem = remoteMem.get();
            if (InjectViaSetWindowsHook(pid, fullPath) ||
                InjectViaQueueUserAPC(pid, loadLibAddr, rawRemoteMem, fullPath)) {
                thisInjected = true;
            }
        }

        if (!thisInjected) {
            lastErr = lastErr ? lastErr : ERROR_FUNCTION_FAILED;
            lastErrMsg = lastErrMsg ? lastErrMsg : "All injection methods failed";
            TraceRuntimeLog("Injector", "ALL injection methods FAILED for pid=" + std::to_string(pid));
            continue;
        }

        // Wait for Two-Way IPC Handshake from OmnilynxInitThreadProc inside target process
        if (hasIpc && ipc.hDone && ipc.view) {
            DWORD waitIpc = WaitForSingleObject(ipc.hDone.get(), 8000);
            if (waitIpc == WAIT_OBJECT_0 && ipc.view->completed && ipc.view->magic == OMNILYNX_IPC_MAGIC) {
                if (!ipc.view->modInitOk || !ipc.view->hooksOk || ipc.view->hooksFailed > 0 || ipc.view->symbolsMissing > 0) {
                    lastErr = ipc.view->win32Error ? ipc.view->win32Error : ERROR_MOD_NOT_FOUND;
                    ipcErrorDetail = std::string("Mod init/hook failed inside target (modInitOk=")
                        + (ipc.view->modInitOk ? "1" : "0")
                        + ", hooksOk=" + (ipc.view->hooksOk ? "1" : "0")
                        + ", hooksFailed=" + std::to_string(ipc.view->hooksFailed)
                        + ", symbolsMissing=" + std::to_string(ipc.view->symbolsMissing) + ")";
                    if (ipc.view->errorMsg[0] != '\0') {
                        ipcErrorDetail += std::string(": ") + ipc.view->errorMsg;
                    }
                    lastErrMsg = ipcErrorDetail.c_str();
                    TraceRuntimeLog("Injector", "IPC Handshake FAILED pid=" + std::to_string(pid) + " -> " + ipcErrorDetail);

                    // Eject failed DLL cleanly so it does not linger in target process
                    HMODULE badMod = FindLoadedModule(pid, fullPath.c_str());
                    if (badMod) {
                        auto freeLibAddr = reinterpret_cast<LPTHREAD_START_ROUTINE>(
                            reinterpret_cast<void*>(GetProcAddress(hKernel32, "FreeLibrary")));
                        if (freeLibAddr) {
                            HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
                            auto pfnNt = hNtdll ? reinterpret_cast<PFN_NtCreateThreadEx>(
                                reinterpret_cast<void*>(GetProcAddress(hNtdll, "NtCreateThreadEx"))) : nullptr;
                            if (pfnNt) {
                                HANDLE ht = nullptr;
                                if (pfnNt(&ht, THREAD_ALL_ACCESS, nullptr, hProcess.get(), freeLibAddr, badMod, 0, 0, 0, 0, nullptr) == 0 && ht) {
                                    WaitForSingleObject(ht, 5000);
                                    CloseHandle(ht);
                                }
                            }
                        }
                    }
                    continue;
                }
                verifiedCount++;
                totalHooksCreated += ipc.view->hooksCreated;
                TraceRuntimeLog("Injector", "IPC Handshake VERIFIED pid=" + std::to_string(pid)
                    + " hooksCreated=" + std::to_string(ipc.view->hooksCreated));
            } else {
                TraceRuntimeLog("Injector", "IPC Handshake timeout/skipped pid=" + std::to_string(pid) + " wait=" + std::to_string(waitIpc));
            }
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

    printf("{\"ok\":true,\"pid\":%lu,\"count\":%d,\"verified\":%s,\"hooksCreated\":%lu,\"injected\":true}\n",
           static_cast<unsigned long>(firstOkPid),
           injectedCount,
           verifiedCount > 0 ? "true" : "false",
           static_cast<unsigned long>(totalHooksCreated));
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

        bool thisEjected = false;
        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (hNtdll) {
            auto pfnNtCreateThreadEx = reinterpret_cast<PFN_NtCreateThreadEx>(
                reinterpret_cast<void*>(GetProcAddress(hNtdll, "NtCreateThreadEx")));
            if (pfnNtCreateThreadEx) {
                HANDLE hThread = nullptr;
                NTSTATUS status = pfnNtCreateThreadEx(
                    &hThread, THREAD_ALL_ACCESS, nullptr, hProcess.get(),
                    freeLibAddr, reinterpret_cast<LPVOID>(foundMod), 0, 0, 0, 0, nullptr);
                if (status == 0 && hThread) {
                    WaitForSingleObject(hThread, 10000);
                    DWORD exitCode = 0;
                    GetExitCodeThread(hThread, &exitCode);
                    CloseHandle(hThread);
                    if (exitCode != 0 || FindLoadedModule(pid, dllName) == nullptr) {
                        thisEjected = true;
                    }
                } else if (hThread) {
                    CloseHandle(hThread);
                }
            }
        }

        if (!thisEjected) {
            ScopedHandle hThread(CreateRemoteThread(
                hProcess.get(), nullptr, 0, freeLibAddr, reinterpret_cast<LPVOID>(foundMod), 0, nullptr));
            if (hThread && WaitForSingleObject(hThread.get(), 10000) == WAIT_OBJECT_0) {
                DWORD exitCode = 0;
                if (GetExitCodeThread(hThread.get(), &exitCode) && (exitCode != 0 || FindLoadedModule(pid, dllName) == nullptr)) {
                    thisEjected = true;
                }
            }
        }

        if (thisEjected) {
            ejectedCount++;
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

int CmdToolMod(const wchar_t* modId, const wchar_t* dllPath) {
    std::wstring fullPath = ResolveFullPathW(dllPath);
    if (fullPath.empty() || GetFileAttributesW(fullPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        TraceRuntimeLog("ToolMod", "DLL not found: " + WideToUtf8(dllPath));
        return 1;
    }

    std::wstring eventName = std::wstring(L"Omnilynx_StopToolMod_") + modId;
    ScopedHandle hStopEvent(CreateEventW(nullptr, TRUE, FALSE, eventName.c_str()));
    if (!hStopEvent) {
        TraceRuntimeLog("ToolMod", "CreateEventW failed");
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        TraceRuntimeLog("ToolMod", "ToolMod already running for " + WideToUtf8(modId));
        return 0;
    }

    GrantAppPackagesReadExecute(fullPath);
    TraceRuntimeLog("ToolMod", "Starting tool-mod host mod=" + WideToUtf8(modId) + " dll=" + WideToUtf8(fullPath));

    HMODULE hMod = LoadLibraryW(fullPath.c_str());
    if (!hMod) {
        TraceRuntimeLog("ToolMod", "LoadLibraryW failed err=" + FormatWin32ErrorUtf8(GetLastError()));
        return 1;
    }

    HANDLE waitHandles[1] = { hStopEvent.get() };
    for (;;) {
        DWORD wait = MsgWaitForMultipleObjects(1, waitHandles, FALSE, INFINITE, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0) {
            break;
        }
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                SetEvent(hStopEvent.get());
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (WaitForSingleObject(hStopEvent.get(), 0) == WAIT_OBJECT_0) {
            break;
        }
    }

    TraceRuntimeLog("ToolMod", "Stopping tool-mod host mod=" + WideToUtf8(modId));
    FreeLibrary(hMod);
    return 0;
}

int CmdStopToolMod(const wchar_t* modId) {
    std::wstring eventName = std::wstring(L"Omnilynx_StopToolMod_") + modId;
    ScopedHandle hStopEvent(OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName.c_str()));
    if (!hStopEvent) {
        printf("{\"ok\":true,\"stopped\":false,\"reason\":\"not_running\"}\n");
        return 0;
    }
    SetEvent(hStopEvent.get());
    printf("{\"ok\":true,\"stopped\":true}\n");
    return 0;
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    (void)EnableSeDebugPrivilege();
    if (argc < 2) {
        printf("{\"ok\":false,\"error\":\"Usage: wendo_injector <list|inject|eject|apply|sysinfo|fetch|install|rollback|-tool-mod|stop-tool> ...\"}\n");
        return 1;
    }

    if (_wcsicmp(argv[1], L"-tool-mod") == 0 && argc >= 4) {
        return CmdToolMod(argv[2], argv[3]);
    }
    if (_wcsicmp(argv[1], L"stop-tool") == 0 && argc >= 3) {
        return CmdStopToolMod(argv[2]);
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
