#pragma once

// Omnilynx Native .wh.cpp Compatibility SDK
// Complete Windhawk Mod API compatibility layer backed by MinHook, HDE64, DbgHelp, and WinHttp.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <knownfolders.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifdef __cplusplus
#include <vector>
#endif
#include "MinHook.h"

#ifndef WH_MOD_ID
#define WH_MOD_ID L"omnilynx-mod"
#endif

#ifndef WH_MOD_VERSION
#define WH_MOD_VERSION L"1.0.0"
#endif

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllexport) inline LRESULT CALLBACK OmnilynxGetMsgProc(int code, WPARAM wParam, LPARAM lParam) {
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

// ============================================================================
// Runtime Trace & Logging
// ============================================================================
static inline void Omnilynx_TraceLog(const char* fmt, ...) {
    wchar_t dir[1024] = {};
    DWORD n = ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Omnilynx", dir, 1024);
    if (n == 0 || n >= 1024) return;
    CreateDirectoryW(dir, nullptr);

    wchar_t logPath[1088] = {};
    _snwprintf_s(logPath, 1088, _TRUNCATE, L"%s\\runtime_trace.log", dir);

    HANDLE hFile = CreateFileW(
        logPath,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME st{};
    GetLocalTime(&st);
    char buf[1536] = {};
    int off = snprintf(
        buf, sizeof(buf),
        "[%04u-%02u-%02u %02u:%02u:%02u.%03u][PID:%lu][ModDLL] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        static_cast<unsigned long>(GetCurrentProcessId()));

    if (off > 0 && static_cast<size_t>(off) < sizeof(buf) - 4) {
        va_list args;
        va_start(args, fmt);
        int msgLen = vsnprintf(buf + off, sizeof(buf) - static_cast<size_t>(off) - 3, fmt, args);
        va_end(args);
        if (msgLen > 0) off += msgLen;
    }
    if (static_cast<size_t>(off) < sizeof(buf) - 2) {
        buf[off++] = '\r';
        buf[off++] = '\n';
    }

    DWORD written = 0;
    WriteFile(hFile, buf, static_cast<DWORD>(off), &written, nullptr);
    CloseHandle(hFile);
}

static inline void Wh_Log(PCWSTR format, ...) {
    if (!format) return;
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, 1024, _TRUNCATE, format, args);
    va_end(args);
    OutputDebugStringW(L"[OmnilynxMod] ");
    OutputDebugStringW(buffer);
    OutputDebugStringW(L"\n");
    Omnilynx_TraceLog("Wh_Log: %ls", buffer);
}

// ============================================================================
// Hooking & Disassembly (MinHook + HDE64)
// ============================================================================
// ============================================================================
// Two-Way IPC Handshake Struct (shared between ModDLL and wendo_injector.exe)
// ============================================================================
#define OMNILYNX_IPC_MAGIC 0x4F4D4E49u // 'OMNI'

typedef struct tagOmnilynxInitHandshake {
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
} OmnilynxInitHandshake;

static volatile LONG g_omnilynxMhInit = 0;
static volatile LONG g_omnilynxHooksCreated = 0;
static volatile LONG g_omnilynxHooksFailed = 0;
static volatile LONG g_omnilynxSymbolsMissing = 0;
static DWORD g_omnilynxLastHookErr = 0;
static char g_omnilynxLastErrorMsg[256] = {};

static inline void Omnilynx_SetLastErrorMsg(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_omnilynxLastErrorMsg, sizeof(g_omnilynxLastErrorMsg), fmt, args);
    va_end(args);
}

static inline BOOL Wh_SetFunctionHook(
    void* targetFunction,
    void* hookFunction,
    void** originalFunction
) {
    if (!targetFunction || !hookFunction) {
        InterlockedIncrement(&g_omnilynxHooksFailed);
        Omnilynx_SetLastErrorMsg("Wh_SetFunctionHook: null target or hook");
        Omnilynx_TraceLog("Wh_SetFunctionHook INVALID args target=%p hook=%p", targetFunction, hookFunction);
        return FALSE;
    }
    if (InterlockedCompareExchange(&g_omnilynxMhInit, 1, 0) == 0) {
        MH_STATUS initSt = MH_Initialize();
        Omnilynx_TraceLog("MH_Initialize() -> %d", static_cast<int>(initSt));
        if (initSt != MH_OK && initSt != MH_ERROR_ALREADY_INITIALIZED) {
            InterlockedExchange(&g_omnilynxMhInit, 0);
            InterlockedIncrement(&g_omnilynxHooksFailed);
            g_omnilynxLastHookErr = static_cast<DWORD>(initSt);
            Omnilynx_SetLastErrorMsg("MH_Initialize failed (%d)", static_cast<int>(initSt));
            return FALSE;
        }
    }
    MH_STATUS st = MH_CreateHook(targetFunction, hookFunction, originalFunction);
    Omnilynx_TraceLog("MH_CreateHook(target=%p, hook=%p) -> %d", targetFunction, hookFunction, static_cast<int>(st));
    if (st == MH_OK || st == MH_ERROR_ALREADY_CREATED) {
        InterlockedIncrement(&g_omnilynxHooksCreated);
        return TRUE;
    }
    InterlockedIncrement(&g_omnilynxHooksFailed);
    g_omnilynxLastHookErr = static_cast<DWORD>(st);
    Omnilynx_SetLastErrorMsg("MH_CreateHook failed (%d)", static_cast<int>(st));
    return FALSE;
}

static inline BOOL Wh_RemoveFunctionHook(void* targetFunction) {
    if (!targetFunction) return FALSE;
    if (InterlockedCompareExchange(&g_omnilynxMhInit, 1, 1) == 0) return FALSE;
    MH_DisableHook(targetFunction);
    MH_STATUS st = MH_RemoveHook(targetFunction);
    Omnilynx_TraceLog("Wh_RemoveFunctionHook(target=%p) -> %d", targetFunction, static_cast<int>(st));
    return (st == MH_OK || st == MH_ERROR_NOT_CREATED) ? TRUE : FALSE;
}

static inline BOOL Wh_ApplyHookOperations(void) {
    if (InterlockedCompareExchange(&g_omnilynxMhInit, 1, 1) == 0) return TRUE;
    MH_STATUS st = MH_EnableHook(MH_ALL_HOOKS);
    Omnilynx_TraceLog("MH_EnableHook(MH_ALL_HOOKS) -> %d", static_cast<int>(st));
    if (st != MH_OK) {
        g_omnilynxLastHookErr = static_cast<DWORD>(st);
        Omnilynx_SetLastErrorMsg("MH_EnableHook failed (%d)", static_cast<int>(st));
        return FALSE;
    }
    return TRUE;
}

static inline void Wh_RemoveAllHooks(void) {
    if (InterlockedExchange(&g_omnilynxMhInit, 0) == 1) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
}

typedef struct tagWH_DISASM_RESULT {
    size_t length;
    char text[96];
} WH_DISASM_RESULT;

typedef struct {
    uint8_t len;
    uint8_t p_rep;
    uint8_t p_lock;
    uint8_t p_seg;
    uint8_t p_66;
    uint8_t p_67;
    uint8_t rex;
    uint8_t rex_w;
    uint8_t rex_r;
    uint8_t rex_x;
    uint8_t rex_b;
    uint8_t opcode;
    uint8_t opcode2;
    uint8_t modrm;
    uint8_t modrm_mod;
    uint8_t modrm_reg;
    uint8_t modrm_rm;
    uint8_t sib;
    uint8_t sib_scale;
    uint8_t sib_index;
    uint8_t sib_base;
    union {
        uint8_t imm8;
        uint16_t imm16;
        uint32_t imm32;
        uint64_t imm64;
    } imm;
    union {
        uint8_t disp8;
        uint16_t disp16;
        uint32_t disp32;
    } disp;
    uint32_t flags;
} omnilynx_hde64s;

unsigned int hde64_disasm(const void* code, omnilynx_hde64s* hs);

static inline BOOL Wh_Disasm(void* address, WH_DISASM_RESULT* result) {
    if (!address || !result) return FALSE;
    memset(result, 0, sizeof(*result));
    omnilynx_hde64s hs{};
    unsigned int len = hde64_disasm(address, &hs);
    if (len == 0 || (hs.flags & 0x00001000u)) {
        return FALSE;
    }
    result->length = len;

    static const char* const regs64[16] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"
    };
    static const char* const regs32[16] = {
        "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
        "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
    };

    const uint8_t op = hs.opcode;
    const uint8_t regIdx = ((hs.rex_r ? 8 : 0) | hs.modrm_reg) & 15;
    const uint8_t rmIdx = ((hs.rex_b ? 8 : 0) | hs.modrm_rm) & 15;
    const char* regName = hs.rex_w ? regs64[regIdx] : regs32[regIdx];
    const char* baseName = regs64[rmIdx];

    if (op == 0xC3 || op == 0xC2) {
        snprintf(result->text, sizeof(result->text), "ret");
    } else if (op == 0xCC) {
        snprintf(result->text, sizeof(result->text), "int3");
    } else if (op == 0x90) {
        snprintf(result->text, sizeof(result->text), "nop");
    } else if (op == 0xE8) {
        int64_t rel = static_cast<int32_t>(hs.imm.imm32);
        uintptr_t target = reinterpret_cast<uintptr_t>(address) + len + rel;
        snprintf(result->text, sizeof(result->text), "call 0x%llx", static_cast<unsigned long long>(target));
    } else if (op == 0xE9 || op == 0xEB) {
        int64_t rel = (op == 0xEB) ? static_cast<int8_t>(hs.imm.imm8) : static_cast<int32_t>(hs.imm.imm32);
        uintptr_t target = reinterpret_cast<uintptr_t>(address) + len + rel;
        snprintf(result->text, sizeof(result->text), "jmp 0x%llx", static_cast<unsigned long long>(target));
    } else if ((op == 0x8B || op == 0x8D) && hs.modrm_mod != 3) {
        const char* mnem = (op == 0x8D) ? "lea" : "mov";
        if (hs.modrm_mod == 0 && hs.modrm_rm != 4 && hs.modrm_rm != 5) {
            snprintf(result->text, sizeof(result->text), "%s %s, [%s]", mnem, regName, baseName);
        } else if (hs.modrm_mod == 1 && hs.modrm_rm != 4) {
            int8_t d = static_cast<int8_t>(hs.disp.disp8);
            if (d >= 0) snprintf(result->text, sizeof(result->text), "%s %s, [%s+0x%x]", mnem, regName, baseName, static_cast<unsigned>(d));
            else snprintf(result->text, sizeof(result->text), "%s %s, [%s-0x%x]", mnem, regName, baseName, static_cast<unsigned>(-d));
        } else if (hs.modrm_mod == 2 && hs.modrm_rm != 4) {
            int32_t d = static_cast<int32_t>(hs.disp.disp32);
            if (d >= 0) snprintf(result->text, sizeof(result->text), "%s %s, [%s+0x%x]", mnem, regName, baseName, static_cast<unsigned>(d));
            else snprintf(result->text, sizeof(result->text), "%s %s, [%s-0x%x]", mnem, regName, baseName, static_cast<unsigned>(-d));
        } else {
            snprintf(result->text, sizeof(result->text), "%s %s, [...]", mnem, regName);
        }
    } else if (op == 0x89 && hs.modrm_mod == 3) {
        snprintf(result->text, sizeof(result->text), "mov %s, %s", hs.rex_w ? regs64[rmIdx] : regs32[rmIdx], regName);
    } else if (op >= 0x50 && op <= 0x57) {
        snprintf(result->text, sizeof(result->text), "push %s", regs64[((hs.rex_b ? 8 : 0) | (op - 0x50)) & 15]);
    } else if (op >= 0x58 && op <= 0x5F) {
        snprintf(result->text, sizeof(result->text), "pop %s", regs64[((hs.rex_b ? 8 : 0) | (op - 0x58)) & 15]);
    } else {
        snprintf(result->text, sizeof(result->text), "db 0x%02x", op);
    }
    return TRUE;
}

// ============================================================================
// Settings & Persistent Storage
// ============================================================================
static inline const wchar_t* Omnilynx_GetSettingsIniPath(wchar_t* outBuf, DWORD bufLen) {
    if (!outBuf || bufLen == 0) return L"";
#ifdef WENDO_SETTINGS_INI
    wcsncpy_s(outBuf, bufLen, WENDO_SETTINGS_INI, _TRUNCATE);
    return outBuf;
#else
    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Omnilynx\\Config\\mod_settings.ini", outBuf, bufLen);
    if (GetFileAttributesW(outBuf) != INVALID_FILE_ATTRIBUTES) {
        return outBuf;
    }
    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Programs\\Omnilynx\\Config\\mod_settings.ini", outBuf, bufLen);
    if (GetFileAttributesW(outBuf) != INVALID_FILE_ATTRIBUTES) {
        return outBuf;
    }
    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Omnilynx\\mod_settings.ini", outBuf, bufLen);
    return outBuf;
#endif
}

static inline BOOL Wh_GetModStoragePath(PWSTR outPath, size_t maxChars) {
    if (!outPath || maxChars == 0) return FALSE;
    wchar_t base[1024] = {};
    DWORD n = ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Omnilynx\\Storage", base, 1024);
    if (n == 0 || n >= 1024) return FALSE;
    CreateDirectoryW(base, nullptr);
    _snwprintf_s(outPath, maxChars, _TRUNCATE, L"%s\\%s", base, WH_MOD_ID);
    CreateDirectoryW(outPath, nullptr);
    return TRUE;
}

static inline void Omnilynx_GetBinaryValuePath(PCWSTR valueName, wchar_t* outPath, size_t maxChars) {
    wchar_t storage[1024] = {};
    if (!Wh_GetModStoragePath(storage, 1024)) {
        outPath[0] = L'\0';
        return;
    }
    wchar_t binDir[1056] = {};
    _snwprintf_s(binDir, 1056, _TRUNCATE, L"%s\\values", storage);
    CreateDirectoryW(binDir, nullptr);

    wchar_t safeName[256] = {};
    size_t idx = 0;
    for (size_t i = 0; valueName && valueName[i] && idx < 250; ++i) {
        wchar_t c = valueName[i];
        if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'-' || c == L'_') {
            safeName[idx++] = c;
        } else {
            safeName[idx++] = L'_';
        }
    }
    safeName[idx] = L'\0';
    _snwprintf_s(outPath, maxChars, _TRUNCATE, L"%s\\%s.bin", binDir, safeName);
}

static inline int Wh_GetIntSetting(PCWSTR valueName, ...) {
    if (!valueName) return 0;
    wchar_t formattedName[256];
    va_list args;
    va_start(args, valueName);
    _vsnwprintf_s(formattedName, 256, _TRUNCATE, valueName, args);
    va_end(args);

    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    return static_cast<int>(GetPrivateProfileIntW(L"Settings", formattedName, 0, iniPath));
}

static inline PCWSTR Wh_GetStringSetting(PCWSTR valueName, ...) {
    if (!valueName) return nullptr;
    wchar_t formattedName[256];
    va_list args;
    va_start(args, valueName);
    _vsnwprintf_s(formattedName, 256, _TRUNCATE, valueName, args);
    va_end(args);

    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    wchar_t* buf = static_cast<wchar_t*>(malloc(1024 * sizeof(wchar_t)));
    if (!buf) return nullptr;
    GetPrivateProfileStringW(L"Settings", formattedName, L"", buf, 1024, iniPath);
    return buf;
}

static inline void Wh_FreeStringSetting(PCWSTR string) {
    if (string) free(const_cast<wchar_t*>(string));
}

static inline void Wh_SetIntSetting(PCWSTR valueName, int value) {
    if (!valueName) return;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    wchar_t buf[32];
    _snwprintf_s(buf, 32, _TRUNCATE, L"%d", value);
    WritePrivateProfileStringW(L"Settings", valueName, buf, iniPath);
}

static inline void Wh_SetStringSetting(PCWSTR valueName, PCWSTR value) {
    if (!valueName) return;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    WritePrivateProfileStringW(L"Settings", valueName, value ? value : L"", iniPath);
}

static inline int Wh_GetIntValue(PCWSTR valueName, int defaultValue) {
    if (!valueName) return defaultValue;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    return static_cast<int>(GetPrivateProfileIntW(L"Values", valueName, defaultValue, iniPath));
}

static inline BOOL Wh_SetIntValue(PCWSTR valueName, int value) {
    if (!valueName) return FALSE;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    wchar_t buf[32];
    _snwprintf_s(buf, 32, _TRUNCATE, L"%d", value);
    return WritePrivateProfileStringW(L"Values", valueName, buf, iniPath);
}

static inline size_t Wh_GetStringValue(PCWSTR valueName, PWSTR stringBuffer, size_t bufferChars) {
    if (!valueName || !stringBuffer || bufferChars == 0) return 0;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    return static_cast<size_t>(
        GetPrivateProfileStringW(L"Values", valueName, L"", stringBuffer, static_cast<DWORD>(bufferChars), iniPath));
}

static inline BOOL Wh_SetStringValue(PCWSTR valueName, PCWSTR value) {
    if (!valueName) return FALSE;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    return WritePrivateProfileStringW(L"Values", valueName, value ? value : L"", iniPath);
}

static inline size_t Wh_GetBinaryValue(PCWSTR valueName, void* buffer, size_t bufferSize) {
    if (!valueName) return 0;
    wchar_t binPath[1088] = {};
    Omnilynx_GetBinaryValuePath(valueName, binPath, 1088);
    if (!binPath[0]) return 0;

    HANDLE hFile = CreateFileW(binPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart <= 0) {
        CloseHandle(hFile);
        return 0;
    }

    size_t total = static_cast<size_t>(fileSize.QuadPart);
    if (!buffer || bufferSize == 0) {
        CloseHandle(hFile);
        return total;
    }

    DWORD toRead = static_cast<DWORD>(total < bufferSize ? total : bufferSize);
    DWORD readBytes = 0;
    ReadFile(hFile, buffer, toRead, &readBytes, nullptr);
    CloseHandle(hFile);
    return static_cast<size_t>(readBytes);
}

static inline BOOL Wh_SetBinaryValue(PCWSTR valueName, const void* buffer, size_t bufferSize) {
    if (!valueName) return FALSE;
    wchar_t binPath[1088] = {};
    Omnilynx_GetBinaryValuePath(valueName, binPath, 1088);
    if (!binPath[0]) return FALSE;

    HANDLE hFile = CreateFileW(binPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    DWORD written = 0;
    BOOL ok = TRUE;
    if (buffer && bufferSize > 0) {
        ok = WriteFile(hFile, buffer, static_cast<DWORD>(bufferSize), &written, nullptr);
    }
    CloseHandle(hFile);
    return ok;
}

static inline BOOL Wh_DeleteValue(PCWSTR valueName) {
    if (!valueName) return FALSE;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    BOOL iniOk = WritePrivateProfileStringW(L"Values", valueName, nullptr, iniPath);

    wchar_t binPath[1088] = {};
    Omnilynx_GetBinaryValuePath(valueName, binPath, 1088);
    BOOL binOk = binPath[0] ? DeleteFileW(binPath) : FALSE;
    return (iniOk || binOk) ? TRUE : FALSE;
}

// ============================================================================
// URL Content Fetcher (Wh_GetUrlContent / Wh_FreeUrlContent)
// ============================================================================
typedef struct tagWH_GET_URL_CONTENT_OPTIONS {
    size_t optionsSize;
    PCWSTR targetFilePath;
} WH_GET_URL_CONTENT_OPTIONS;

typedef struct tagWH_URL_CONTENT {
    const char* data;
    size_t length;
    int statusCode;
} WH_URL_CONTENT;

static inline const WH_URL_CONTENT* Wh_GetUrlContent(
    PCWSTR url,
    const WH_GET_URL_CONTENT_OPTIONS* options)
{
    if (!url) return nullptr;
    HMODULE hWinHttp = LoadLibraryW(L"winhttp.dll");
    if (!hWinHttp) return nullptr;

    using WinHttpOpenFn = void*(WINAPI*)(PCWSTR, DWORD, PCWSTR, PCWSTR, DWORD);
    using WinHttpConnectFn = void*(WINAPI*)(void*, PCWSTR, WORD, DWORD);
    using WinHttpOpenRequestFn = void*(WINAPI*)(void*, PCWSTR, PCWSTR, PCWSTR, PCWSTR, PCWSTR*, DWORD);
    using WinHttpSendRequestFn = BOOL(WINAPI*)(void*, PCWSTR, DWORD, void*, DWORD, DWORD, DWORD_PTR);
    using WinHttpReceiveResponseFn = BOOL(WINAPI*)(void*, void*);
    using WinHttpQueryHeadersFn = BOOL(WINAPI*)(void*, DWORD, PCWSTR, void*, DWORD*, DWORD*);
    using WinHttpReadDataFn = BOOL(WINAPI*)(void*, void*, DWORD, DWORD*);
    using WinHttpCloseHandleFn = BOOL(WINAPI*)(void*);
    using WinHttpCrackUrlFn = BOOL(WINAPI*)(PCWSTR, DWORD, DWORD, void*);

    struct URL_COMPONENTS_RAW {
        DWORD dwStructSize;
        LPWSTR lpszScheme;
        DWORD dwSchemeLength;
        int nScheme;
        LPWSTR lpszHostName;
        DWORD dwHostNameLength;
        WORD nPort;
        LPWSTR lpszUserName;
        DWORD dwUserNameLength;
        LPWSTR lpszPassword;
        DWORD dwPasswordLength;
        LPWSTR lpszUrlPath;
        DWORD dwUrlPathLength;
        LPWSTR lpszExtraInfo;
        DWORD dwExtraInfoLength;
    };

    auto fnOpen = reinterpret_cast<WinHttpOpenFn>(reinterpret_cast<void*>(GetProcAddress(hWinHttp, "WinHttpOpen")));
    auto fnConnect = reinterpret_cast<WinHttpConnectFn>(reinterpret_cast<void*>(GetProcAddress(hWinHttp, "WinHttpConnect")));
    auto fnOpenReq = reinterpret_cast<WinHttpOpenRequestFn>(reinterpret_cast<void*>(GetProcAddress(hWinHttp, "WinHttpOpenRequest")));
    auto fnSend = reinterpret_cast<WinHttpSendRequestFn>(reinterpret_cast<void*>(GetProcAddress(hWinHttp, "WinHttpSendRequest")));
    auto fnRecv = reinterpret_cast<WinHttpReceiveResponseFn>(reinterpret_cast<void*>(GetProcAddress(hWinHttp, "WinHttpReceiveResponse")));
    auto fnQuery = reinterpret_cast<WinHttpQueryHeadersFn>(reinterpret_cast<void*>(GetProcAddress(hWinHttp, "WinHttpQueryHeaders")));
    auto fnRead = reinterpret_cast<WinHttpReadDataFn>(reinterpret_cast<void*>(GetProcAddress(hWinHttp, "WinHttpReadData")));
    auto fnClose = reinterpret_cast<WinHttpCloseHandleFn>(reinterpret_cast<void*>(GetProcAddress(hWinHttp, "WinHttpCloseHandle")));
    auto fnCrack = reinterpret_cast<WinHttpCrackUrlFn>(reinterpret_cast<void*>(GetProcAddress(hWinHttp, "WinHttpCrackUrl")));
    if (!fnOpen || !fnConnect || !fnOpenReq || !fnSend || !fnRecv || !fnQuery || !fnRead || !fnClose || !fnCrack) {
        return nullptr;
    }

    wchar_t host[256] = {};
    wchar_t pathBuf[2048] = {};
    URL_COMPONENTS_RAW uc{};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = pathBuf;
    uc.dwUrlPathLength = 2048;
    if (!fnCrack(url, 0, 0, &uc)) return nullptr;

    void* hSession = fnOpen(L"Omnilynx/1.0", 0, nullptr, nullptr, 0);
    if (!hSession) return nullptr;
    void* hConnect = fnConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { fnClose(hSession); return nullptr; }
    DWORD flags = (uc.nScheme == 2) ? 0x00800000u : 0u; // WINHTTP_FLAG_SECURE
    void* hReq = fnOpenReq(hConnect, L"GET", pathBuf[0] ? pathBuf : L"/", nullptr, nullptr, nullptr, flags);
    if (!hReq) { fnClose(hConnect); fnClose(hSession); return nullptr; }

    WH_URL_CONTENT* res = nullptr;
    if (fnSend(hReq, nullptr, 0, nullptr, 0, 0, 0) && fnRecv(hReq, nullptr)) {
        DWORD statusCode = 0;
        DWORD scSize = sizeof(statusCode);
        fnQuery(hReq, 19u | 0x20000000u, nullptr, &statusCode, &scSize, nullptr); // WINHTTP_QUERY_STATUS_CODE | FLAG_NUMBER

        char* data = nullptr;
        size_t total = 0;
        char chunk[4096];
        DWORD readBytes = 0;
        while (fnRead(hReq, chunk, sizeof(chunk), &readBytes) && readBytes > 0) {
            char* grown = static_cast<char*>(realloc(data, total + readBytes + 1));
            if (!grown) break;
            data = grown;
            memcpy(data + total, chunk, readBytes);
            total += readBytes;
            data[total] = '\0';
        }
        if (options && options->targetFilePath && data) {
            HANDLE hf = CreateFileW(options->targetFilePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hf != INVALID_HANDLE_VALUE) {
                DWORD w = 0;
                WriteFile(hf, data, static_cast<DWORD>(total), &w, nullptr);
                CloseHandle(hf);
            }
        }
        res = static_cast<WH_URL_CONTENT*>(calloc(1, sizeof(WH_URL_CONTENT)));
        if (res) {
            res->data = data;
            res->length = total;
            res->statusCode = static_cast<int>(statusCode);
        } else {
            free(data);
        }
    }
    fnClose(hReq);
    fnClose(hConnect);
    fnClose(hSession);
    return res;
}

static inline void Wh_FreeUrlContent(const WH_URL_CONTENT* content) {
    if (!content) return;
    free(const_cast<char*>(content->data));
    free(const_cast<WH_URL_CONTENT*>(content));
}

// ============================================================================
// Lifecycle Hooks
// ============================================================================
BOOL Wh_ModInit(void);

#ifdef __GNUC__
__attribute__((weak)) void Wh_ModUninit(void);
__attribute__((weak)) BOOL Wh_ModBeforeInit(void);
__attribute__((weak)) void Wh_ModAfterInit(void);
__attribute__((weak)) void Wh_ModBeforeUninit(void);
__attribute__((weak)) void Wh_ModSettingsChanged(void);
#else
void Wh_ModUninit(void);
void Wh_ModSettingsChanged(void);
#endif

// ============================================================================
// Native DbgHelp PDB Symbol Resolution (Wh_FindFirstSymbol* / Wh_HookSymbols)
// ============================================================================
typedef struct WH_FIND_SYMBOL_OPTIONS {
    size_t optionsSize;
    BOOL noUnmangle;
} WH_FIND_SYMBOL_OPTIONS;

typedef struct WH_FIND_SYMBOL {
    void* address;
    PCWSTR symbol;
    PCWSTR symbolDecorated;
} WH_FIND_SYMBOL;

typedef struct WH_SYMBOL_HOOK {
    const wchar_t* const* symbols;
    size_t symbolsCount;
    void** pOriginalFunction;
    void* hookFunction;
    bool optional;
} WH_SYMBOL_HOOK;

typedef struct WH_HOOK_SYMBOLS_OPTIONS {
    size_t optionsSize;
    PCWSTR symbolServer;
    bool noCache;
} WH_HOOK_SYMBOLS_OPTIONS;

namespace omnilynx_pdb {

#ifndef SYMOPT_UNDNAME
#define SYMOPT_UNDNAME        0x00000002
#endif
#ifndef SYMOPT_DEFERRED_LOADS
#define SYMOPT_DEFERRED_LOADS 0x00000004
#endif

#pragma pack(push, 8)
struct SYMBOL_INFOW_RAW {
    ULONG SizeOfStruct;
    ULONG TypeIndex;
    ULONG64 Reserved[2];
    ULONG Index;
    ULONG Size;
    ULONG64 ModBase;
    ULONG Flags;
    ULONG64 Value;
    ULONG64 Address;
    ULONG Register;
    ULONG Scope;
    ULONG Tag;
    ULONG NameLen;
    ULONG MaxNameLen;
    WCHAR Name[1];
};
#pragma pack(pop)

using SymSetOptionsFn = DWORD(WINAPI*)(DWORD);
using SymInitializeWFn = BOOL(WINAPI*)(HANDLE, PCWSTR, BOOL);
using SymSetSearchPathWFn = BOOL(WINAPI*)(HANDLE, PCWSTR);
using SymLoadModuleExWFn = DWORD64(WINAPI*)(HANDLE, HANDLE, PCWSTR, PCWSTR, DWORD64, DWORD, void*, DWORD);
using SymEnumSymbolsProcW = BOOL(CALLBACK*)(SYMBOL_INFOW_RAW*, ULONG, PVOID);
using SymEnumSymbolsWFn = BOOL(WINAPI*)(HANDLE, ULONG64, PCWSTR, SymEnumSymbolsProcW, PVOID);

struct SymEntry {
    void* address;
    wchar_t name[256];
};

struct SymSearchState {
    SymEntry* entries;
    size_t count;
    size_t index;
};

static inline HMODULE GetDbgHelp() noexcept {
    static HMODULE hDbg = LoadLibraryW(L"dbghelp.dll");
    return hDbg;
}

static inline bool EnsureSymInitialized(PCWSTR customServer = nullptr) noexcept {
    static volatile LONG s_init = 0;
    wchar_t symCache[1024] = {};
    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Omnilynx\\symbols", symCache, 1024);
    CreateDirectoryW(symCache, nullptr);

    PCWSTR server = (customServer && customServer[0]) ? customServer : L"https://msdl.microsoft.com/download/symbols";
    wchar_t symPath[1280] = {};
    _snwprintf_s(symPath, 1280, _TRUNCATE, L"srv*%s*%s", symCache, server);

    if (InterlockedCompareExchange(&s_init, 1, 0) == 0) {
        HMODULE hDbg = GetDbgHelp();
        if (!hDbg) {
            InterlockedExchange(&s_init, 0);
            return false;
        }
        auto symSetOpts = reinterpret_cast<SymSetOptionsFn>(reinterpret_cast<void*>(GetProcAddress(hDbg, "SymSetOptions")));
        auto symInitW = reinterpret_cast<SymInitializeWFn>(reinterpret_cast<void*>(GetProcAddress(hDbg, "SymInitializeW")));
        if (!symInitW) {
            InterlockedExchange(&s_init, 0);
            return false;
        }
        if (symSetOpts) {
            symSetOpts(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        }
        if (!symInitW(GetCurrentProcess(), symPath, FALSE)) {
            InterlockedExchange(&s_init, 0);
            return false;
        }
    } else if (customServer && customServer[0]) {
        HMODULE hDbg = GetDbgHelp();
        auto symSetPath = reinterpret_cast<SymSetSearchPathWFn>(reinterpret_cast<void*>(GetProcAddress(hDbg, "SymSetSearchPathW")));
        if (symSetPath) symSetPath(GetCurrentProcess(), symPath);
    }
    return true;
}

static inline DWORD64 LoadModuleSymbols(HMODULE hModule, PCWSTR customServer = nullptr) noexcept {
    if (!EnsureSymInitialized(customServer)) return 0;
    HMODULE hDbg = GetDbgHelp();
    auto symLoad = reinterpret_cast<SymLoadModuleExWFn>(reinterpret_cast<void*>(GetProcAddress(hDbg, "SymLoadModuleExW")));
    if (!symLoad) return 0;

    wchar_t modPath[1024] = {};
    if (!GetModuleFileNameW(hModule, modPath, 1024)) return 0;
    return symLoad(GetCurrentProcess(), nullptr, modPath, nullptr, reinterpret_cast<DWORD64>(hModule), 0, nullptr, 0);
}

static inline void GetSymbolCacheFile(wchar_t* outBuf, DWORD chars) noexcept {
    wchar_t dir[1024] = {};
    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Omnilynx\\symbols", dir, 1024);
    CreateDirectoryW(dir, nullptr);
    _snwprintf_s(outBuf, chars, _TRUNCATE, L"%s\\cache.ini", dir);
}

static inline DWORD GetOsBuildNumber() noexcept {
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOEXW*);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return 0;
    auto fn = reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(GetProcAddress(hNtdll, "RtlGetVersion")));
    if (!fn) return 0;
    OSVERSIONINFOEXW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    return (fn(&osvi) == 0) ? osvi.dwBuildNumber : 0;
}

} // namespace omnilynx_pdb

static inline HANDLE Wh_FindFirstSymbol(
    HMODULE hModule,
    const WH_FIND_SYMBOL_OPTIONS* options,
    WH_FIND_SYMBOL* findData)
{
    if (!hModule || !findData) return nullptr;
    DWORD64 modBase = omnilynx_pdb::LoadModuleSymbols(hModule);
    if (!modBase) return nullptr;

    HMODULE hDbg = omnilynx_pdb::GetDbgHelp();
    auto symEnum = reinterpret_cast<omnilynx_pdb::SymEnumSymbolsWFn>(
        reinterpret_cast<void*>(GetProcAddress(hDbg, "SymEnumSymbolsW")));
    if (!symEnum) return nullptr;

    if (options && options->noUnmangle) {
        auto symSetOpts = reinterpret_cast<omnilynx_pdb::SymSetOptionsFn>(
            reinterpret_cast<void*>(GetProcAddress(hDbg, "SymSetOptions")));
        if (symSetOpts) symSetOpts(SYMOPT_DEFERRED_LOADS);
    }

    auto* state = static_cast<omnilynx_pdb::SymSearchState*>(calloc(1, sizeof(omnilynx_pdb::SymSearchState)));
    if (!state) return nullptr;

    struct CollectCtx {
        omnilynx_pdb::SymSearchState* state;
        size_t cap;
    } ctx{ state, 0 };

    auto enumCb = [](omnilynx_pdb::SYMBOL_INFOW_RAW* pSym, ULONG, PVOID userCtx) -> BOOL {
        auto* c = static_cast<CollectCtx*>(userCtx);
        if (!pSym || !pSym->Address) return TRUE;
        if (c->state->count >= c->cap) {
            size_t newCap = c->cap ? c->cap * 2 : 256;
            auto* grown = static_cast<omnilynx_pdb::SymEntry*>(
                realloc(c->state->entries, newCap * sizeof(omnilynx_pdb::SymEntry)));
            if (!grown) return FALSE;
            c->state->entries = grown;
            c->cap = newCap;
        }
        omnilynx_pdb::SymEntry& e = c->state->entries[c->state->count++];
        e.address = reinterpret_cast<void*>(static_cast<uintptr_t>(pSym->Address));
        wcsncpy_s(e.name, 256, pSym->Name, _TRUNCATE);
        return TRUE;
    };

    symEnum(GetCurrentProcess(), modBase, L"*", enumCb, &ctx);
    if (state->count == 0) {
        free(state->entries);
        free(state);
        return nullptr;
    }

    findData->address = state->entries[0].address;
    findData->symbol = state->entries[0].name;
    findData->symbolDecorated = state->entries[0].name;
    state->index = 1;
    return reinterpret_cast<HANDLE>(state);
}

static inline BOOL Wh_FindNextSymbol(HANDLE symSearch, WH_FIND_SYMBOL* findData) {
    if (!symSearch || !findData) return FALSE;
    auto* state = reinterpret_cast<omnilynx_pdb::SymSearchState*>(symSearch);
    if (state->index >= state->count) return FALSE;
    omnilynx_pdb::SymEntry& e = state->entries[state->index++];
    findData->address = e.address;
    findData->symbol = e.name;
    findData->symbolDecorated = e.name;
    return TRUE;
}

static inline HANDLE Wh_FindFirstSymbol2(
    HMODULE hModule,
    const WH_FIND_SYMBOL_OPTIONS* options,
    WH_FIND_SYMBOL* findData)
{
    return Wh_FindFirstSymbol(hModule, options, findData);
}

static inline BOOL Wh_FindNextSymbol2(HANDLE symSearch, WH_FIND_SYMBOL* findData) {
    return Wh_FindNextSymbol(symSearch, findData);
}

static inline void Wh_FindCloseSymbol(HANDLE symSearch) {
    if (!symSearch) return;
    auto* state = reinterpret_cast<omnilynx_pdb::SymSearchState*>(symSearch);
    free(state->entries);
    free(state);
}

static inline bool Wh_HookSymbols(
    HMODULE module,
    const WH_SYMBOL_HOOK* symbolHooks,
    size_t symbolHooksCount,
    const WH_HOOK_SYMBOLS_OPTIONS* options = nullptr)
{
    if (!module || !symbolHooks || symbolHooksCount == 0) return false;

    wchar_t modName[MAX_PATH] = {};
    GetModuleFileNameW(module, modName, MAX_PATH);
    const wchar_t* baseName = wcsrchr(modName, L'\\');
    baseName = baseName ? baseName + 1 : modName;

    wchar_t cacheIni[1024] = {};
    omnilynx_pdb::GetSymbolCacheFile(cacheIni, 1024);
    DWORD osBuild = omnilynx_pdb::GetOsBuildNumber();

    bool useCache = !(options && options->noCache);
    size_t resolvedCount = 0;

    std::vector<void*> resolvedAddrs(symbolHooksCount, nullptr);
    if (useCache) {
        for (size_t i = 0; i < symbolHooksCount; ++i) {
            if (!symbolHooks[i].symbols || symbolHooks[i].symbolsCount == 0) continue;
            wchar_t key[320] = {};
            _snwprintf_s(key, 320, _TRUNCATE, L"%lu_%s_%s",
                         static_cast<unsigned long>(osBuild), baseName, symbolHooks[i].symbols[0]);
            wchar_t rvaBuf[32] = {};
            if (GetPrivateProfileStringW(L"RVACache", key, L"", rvaBuf, 32, cacheIni) > 0) {
                unsigned long long rva = wcstoull(rvaBuf, nullptr, 16);
                if (rva > 0) {
                    resolvedAddrs[i] = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(module) + static_cast<uintptr_t>(rva));
                    resolvedCount++;
                }
            }
        }
    }

    if (resolvedCount < symbolHooksCount) {
        if (options && options->symbolServer) {
            omnilynx_pdb::EnsureSymInitialized(options->symbolServer);
        }
        WH_FIND_SYMBOL sym{};
        HANDLE hSearch = Wh_FindFirstSymbol(module, nullptr, &sym);
        if (hSearch) {
            do {
                for (size_t i = 0; i < symbolHooksCount; ++i) {
                    if (resolvedAddrs[i] || !symbolHooks[i].symbols) continue;
                    for (size_t j = 0; j < symbolHooks[i].symbolsCount; ++j) {
                        const wchar_t* targetSym = symbolHooks[i].symbols[j];
                        if (targetSym && (_wcsicmp(sym.symbol, targetSym) == 0 || wcsstr(sym.symbol, targetSym) != nullptr)) {
                            resolvedAddrs[i] = sym.address;
                            resolvedCount++;
                            if (useCache && sym.address) {
                                uintptr_t rva = reinterpret_cast<uintptr_t>(sym.address) - reinterpret_cast<uintptr_t>(module);
                                wchar_t key[320] = {};
                                _snwprintf_s(key, 320, _TRUNCATE, L"%lu_%s_%s",
                                             static_cast<unsigned long>(osBuild), baseName, symbolHooks[i].symbols[0]);
                                wchar_t rvaBuf[32] = {};
                                _snwprintf_s(rvaBuf, 32, _TRUNCATE, L"%llx", static_cast<unsigned long long>(rva));
                                WritePrivateProfileStringW(L"RVACache", key, rvaBuf, cacheIni);
                            }
                            break;
                        }
                    }
                }
            } while (Wh_FindNextSymbol(hSearch, &sym));
            Wh_FindCloseSymbol(hSearch);
        }
    }

    Omnilynx_TraceLog("Wh_HookSymbols module=%ls resolved=%zu/%zu", baseName, resolvedCount, symbolHooksCount);
    bool allRequiredOk = true;
    for (size_t i = 0; i < symbolHooksCount; ++i) {
        const wchar_t* symName = (symbolHooks[i].symbols && symbolHooks[i].symbolsCount > 0) ? symbolHooks[i].symbols[0] : L"(unknown)";
        if (!resolvedAddrs[i]) {
            Omnilynx_TraceLog("  Symbol NOT FOUND: %ls (optional=%d)", symName, symbolHooks[i].optional ? 1 : 0);
            if (!symbolHooks[i].optional) {
                allRequiredOk = false;
                InterlockedIncrement(&g_omnilynxSymbolsMissing);
                Omnilynx_SetLastErrorMsg("Required symbol not found: %ls", symName);
            }
            continue;
        }
        Omnilynx_TraceLog("  Symbol RESOLVED: %ls -> %p", symName, resolvedAddrs[i]);
        if (symbolHooks[i].hookFunction) {
            if (!Wh_SetFunctionHook(resolvedAddrs[i], symbolHooks[i].hookFunction, symbolHooks[i].pOriginalFunction)) {
                if (!symbolHooks[i].optional) allRequiredOk = false;
            }
        } else if (symbolHooks[i].pOriginalFunction) {
            *symbolHooks[i].pOriginalFunction = resolvedAddrs[i];
        }
    }
    return allRequiredOk;
}

#ifdef __cplusplus
}
#endif

// ============================================================================
// Standard DllMain Entry Point (initializes outside OS Loader Lock)
// ============================================================================
#ifndef WENDO_NO_DLLMAIN
static HANDLE g_omnilynxInitThread = nullptr;
static volatile LONG g_omnilynxUnloading = 0;

static inline void Omnilynx_SignalHandshake(BOOL modInitOk, BOOL hooksOk) {
    DWORD pid = GetCurrentProcessId();
    wchar_t mapName[256] = {};
    wchar_t evtName[256] = {};
    _snwprintf_s(mapName, 256, _TRUNCATE, L"Local\\Omnilynx_IPC_%lu_%ls", static_cast<unsigned long>(pid), WH_MOD_ID);
    _snwprintf_s(evtName, 256, _TRUNCATE, L"Local\\Omnilynx_IPC_Done_%lu_%ls", static_cast<unsigned long>(pid), WH_MOD_ID);

    HANDLE hMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mapName);
    if (hMap) {
        auto* hs = static_cast<OmnilynxInitHandshake*>(MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(OmnilynxInitHandshake)));
        if (hs) {
            hs->magic = OMNILYNX_IPC_MAGIC;
            hs->pid = pid;
            hs->modInitOk = modInitOk;
            hs->hooksOk = hooksOk;
            hs->hooksCreated = static_cast<DWORD>(g_omnilynxHooksCreated);
            hs->hooksFailed = static_cast<DWORD>(g_omnilynxHooksFailed);
            hs->symbolsMissing = static_cast<DWORD>(g_omnilynxSymbolsMissing);
            hs->win32Error = g_omnilynxLastHookErr;
            strncpy_s(hs->errorMsg, sizeof(hs->errorMsg), g_omnilynxLastErrorMsg, _TRUNCATE);
            hs->completed = TRUE;
            UnmapViewOfFile(hs);
        }
        CloseHandle(hMap);
    }

    HANDLE hEvt = OpenEventW(EVENT_MODIFY_STATE, FALSE, evtName);
    if (hEvt) {
        SetEvent(hEvt);
        CloseHandle(hEvt);
    }
}

static DWORD WINAPI OmnilynxInitThreadProc(LPVOID) {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    Omnilynx_TraceLog("OmnilynxInitThreadProc started in host=%ls", exePath);

    BOOL initOk = FALSE;
    BOOL hooksOk = FALSE;

    if (InterlockedCompareExchange(&g_omnilynxUnloading, 0, 0) == 0) {
#ifdef __GNUC__
        if (Wh_ModBeforeInit) {
            Omnilynx_TraceLog("Calling Wh_ModBeforeInit()");
            if (!Wh_ModBeforeInit()) {
                Omnilynx_TraceLog("Wh_ModBeforeInit() returned FALSE, aborting init");
                Omnilynx_SetLastErrorMsg("Wh_ModBeforeInit returned FALSE");
                Omnilynx_SignalHandshake(FALSE, FALSE);
                return 0;
            }
        }
#endif
        initOk = Wh_ModInit();
        Omnilynx_TraceLog("Wh_ModInit() returned %d", static_cast<int>(initOk));
        if (initOk) {
            hooksOk = Wh_ApplyHookOperations();
            Omnilynx_TraceLog("Wh_ApplyHookOperations() returned %d", hooksOk ? 1 : 0);
#ifdef __GNUC__
            if (Wh_ModAfterInit) {
                Omnilynx_TraceLog("Calling Wh_ModAfterInit()");
                Wh_ModAfterInit();
            }
#endif
        } else if (g_omnilynxLastErrorMsg[0] == '\0') {
            Omnilynx_SetLastErrorMsg("Wh_ModInit returned FALSE");
        }
    }
    Omnilynx_SignalHandshake(initOk, hooksOk);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            wchar_t skipBuf[8] = {};
            if (GetEnvironmentVariableW(L"OMNILYNX_INJECTOR_NO_INIT", skipBuf, 8) > 0 && skipBuf[0] == L'1') {
                break;
            }
            Omnilynx_TraceLog("DLL_PROCESS_ATTACH mod=%ls", WH_MOD_ID);
            InterlockedExchange(&g_omnilynxUnloading, 0);
            g_omnilynxInitThread = CreateThread(nullptr, 0, OmnilynxInitThreadProc, nullptr, 0, nullptr);
            if (!g_omnilynxInitThread) {
                Omnilynx_TraceLog("CreateThread(OmnilynxInitThreadProc) FAILED err=%lu", static_cast<unsigned long>(GetLastError()));
            }
            break;
        }
        case DLL_PROCESS_DETACH: {
            InterlockedExchange(&g_omnilynxUnloading, 1);
            if (g_omnilynxInitThread) {
                if (lpReserved == nullptr) {
                    WaitForSingleObject(g_omnilynxInitThread, 3000);
                }
                CloseHandle(g_omnilynxInitThread);
                g_omnilynxInitThread = nullptr;
            }
#ifdef __GNUC__
            if (Wh_ModBeforeUninit) Wh_ModBeforeUninit();
#endif
            Wh_RemoveAllHooks();
#ifdef __GNUC__
            if (Wh_ModUninit) Wh_ModUninit();
#else
            Wh_ModUninit();
#endif
            break;
        }
    }
    return TRUE;
}
#endif
