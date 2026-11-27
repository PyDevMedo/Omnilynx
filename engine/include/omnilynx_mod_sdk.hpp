#pragma once

// Omnilynx Native .wh.cpp Compatibility SDK
// Maps Windhawk Wh_* APIs cleanly onto MinHook + Win32 APIs with RAII safety and zero bloat.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
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

// Logging helper (writes to OutputDebugStringW)
static inline void Wh_Log(const wchar_t* format, ...) {
    if (!format) return;
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, 1024, _TRUNCATE, format, args);
    va_end(args);
    OutputDebugStringW(L"[OmnilynxMod] ");
    OutputDebugStringW(buffer);
    OutputDebugStringW(L"\n");
}

static volatile LONG g_omnilynxMhInit = 0;

static inline bool Wh_SetFunctionHook(
    void* targetFunction,
    void* hookFunction,
    void** originalFunction
) {
    if (!targetFunction || !hookFunction) {
        return false;
    }
    if (InterlockedCompareExchange(&g_omnilynxMhInit, 1, 0) == 0) {
        if (MH_Initialize() != MH_OK) {
            InterlockedExchange(&g_omnilynxMhInit, 0);
            return false;
        }
    }
    MH_STATUS st = MH_CreateHook(targetFunction, hookFunction, originalFunction);
    return (st == MH_OK || st == MH_ERROR_ALREADY_CREATED);
}

static inline bool Wh_ApplyHookOperations(void) {
    if (InterlockedCompareExchange(&g_omnilynxMhInit, 1, 1) == 0) return true;
    return MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
}

static inline void Wh_RemoveAllHooks(void) {
    if (InterlockedExchange(&g_omnilynxMhInit, 0) == 1) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
}

// Resolve %LOCALAPPDATA%\Omnilynx\Config\mod_settings.ini
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

// Per-mod storage path: %LOCALAPPDATA%\Omnilynx\Storage\<WH_MOD_ID>
static inline BOOL Wh_GetModStoragePath(wchar_t* outPath, size_t maxChars) {
    if (!outPath || maxChars == 0) return FALSE;
    wchar_t base[1024] = {};
    DWORD n = ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Omnilynx\\Storage", base, 1024);
    if (n == 0 || n >= 1024) return FALSE;
    CreateDirectoryW(base, nullptr);
    _snwprintf_s(outPath, maxChars, _TRUNCATE, L"%s\\%s", base, WH_MOD_ID);
    CreateDirectoryW(outPath, nullptr);
    return TRUE;
}

static inline int Wh_GetIntSetting(const wchar_t* valueName, ...) {
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

static inline const wchar_t* Wh_GetStringSetting(const wchar_t* valueName, ...) {
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

static inline void Wh_FreeStringSetting(const wchar_t* string) {
    if (string) free(const_cast<wchar_t*>(string));
}

static inline void Wh_SetIntSetting(const wchar_t* valueName, int value) {
    if (!valueName) return;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    wchar_t buf[32];
    _snwprintf_s(buf, 32, _TRUNCATE, L"%d", value);
    WritePrivateProfileStringW(L"Settings", valueName, buf, iniPath);
}

static inline void Wh_SetStringSetting(const wchar_t* valueName, const wchar_t* value) {
    if (!valueName) return;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    WritePrivateProfileStringW(L"Settings", valueName, value ? value : L"", iniPath);
}

static inline int Wh_GetIntValue(const wchar_t* valueName, int defaultValue) {
    if (!valueName) return defaultValue;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    return static_cast<int>(GetPrivateProfileIntW(L"Values", valueName, defaultValue, iniPath));
}

static inline void Wh_SetIntValue(const wchar_t* valueName, int value) {
    if (!valueName) return;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    wchar_t buf[32];
    _snwprintf_s(buf, 32, _TRUNCATE, L"%d", value);
    WritePrivateProfileStringW(L"Values", valueName, buf, iniPath);
}

static inline size_t Wh_GetStringValue(const wchar_t* valueName, wchar_t* stringBuffer, size_t bufferChars) {
    if (!valueName || !stringBuffer || bufferChars == 0) return 0;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    return static_cast<size_t>(
        GetPrivateProfileStringW(L"Values", valueName, L"", stringBuffer, static_cast<DWORD>(bufferChars), iniPath));
}

static inline void Wh_SetStringValue(const wchar_t* valueName, const wchar_t* value) {
    if (!valueName) return;
    wchar_t iniPath[1024];
    Omnilynx_GetSettingsIniPath(iniPath, 1024);
    WritePrivateProfileStringW(L"Values", valueName, value ? value : L"", iniPath);
}

// Optional lifecycle hooks (weakly declared so mods may omit them without redefinition errors)
BOOL Wh_ModInit(void);
void Wh_ModUninit(void);

#ifdef __GNUC__
__attribute__((weak)) void Wh_ModBeforeInit(void);
__attribute__((weak)) void Wh_ModAfterInit(void);
__attribute__((weak)) void Wh_ModSettingsChanged(void);
#else
void Wh_ModSettingsChanged(void);
#endif

#ifdef __cplusplus
}
#endif

// Standard DllMain entry point for Omnilynx mods (initializes outside OS Loader Lock, race-free unload)
#ifndef WENDO_NO_DLLMAIN
static HANDLE g_omnilynxInitThread = nullptr;
static volatile LONG g_omnilynxUnloading = 0;

static DWORD WINAPI OmnilynxInitThreadProc(LPVOID) {
    if (InterlockedCompareExchange(&g_omnilynxUnloading, 0, 0) == 0) {
#ifdef __GNUC__
        if (Wh_ModBeforeInit) Wh_ModBeforeInit();
#endif
        if (Wh_ModInit()) {
            Wh_ApplyHookOperations();
#ifdef __GNUC__
            if (Wh_ModAfterInit) Wh_ModAfterInit();
#endif
        }
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            InterlockedExchange(&g_omnilynxUnloading, 0);
            g_omnilynxInitThread = CreateThread(nullptr, 0, OmnilynxInitThreadProc, nullptr, 0, nullptr);
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
            Wh_RemoveAllHooks();
            Wh_ModUninit();
            break;
        }
    }
    return TRUE;
}
#endif
