#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "MinHook.h"

#ifdef __cplusplus
extern "C" {
#endif

// Logging helper (writes to OutputDebugStringW)
static inline void Wh_Log(const wchar_t* format, ...) {
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, 1024, _TRUNCATE, format, args);
    va_end(args);
    OutputDebugStringW(L"[OmnilynxMod] ");
    OutputDebugStringW(buffer);
    OutputDebugStringW(L"\n");
}

static bool g_mhInitialized = false;

static inline bool Wh_SetFunctionHook(
    void* targetFunction,
    void* hookFunction,
    void** originalFunction
) {
    if (!targetFunction || !hookFunction) {
        return false;
    }
    if (!g_mhInitialized) {
        if (MH_Initialize() != MH_OK) {
            return false;
        }
        g_mhInitialized = true;
    }
    MH_STATUS st = MH_CreateHook(targetFunction, hookFunction, originalFunction);
    return (st == MH_OK || st == MH_ERROR_ALREADY_CREATED);
}

static inline bool Wh_ApplyHookOperations(void) {
    if (!g_mhInitialized) return true;
    return MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
}

static inline void Wh_RemoveAllHooks(void) {
    if (g_mhInitialized) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        g_mhInitialized = false;
    }
}

// Settings helpers (reads from WENDO_SETTINGS_INI passed at compile time, or %LOCALAPPDATA%\Programs\Omnilynx\Config\mod_settings.ini)
static inline const wchar_t* Wendo_GetSettingsIniPath(wchar_t* outBuf, DWORD bufLen) {
#ifdef WENDO_SETTINGS_INI
    wcsncpy_s(outBuf, bufLen, WENDO_SETTINGS_INI, _TRUNCATE);
    return outBuf;
#else
    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Programs\\Omnilynx\\Config\\mod_settings.ini", outBuf, bufLen);
    if (GetFileAttributesW(outBuf) != INVALID_FILE_ATTRIBUTES) {
        return outBuf;
    }
    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Omnilynx\\mod_settings.ini", outBuf, bufLen);
    return outBuf;
#endif
}

static inline int Wh_GetIntSetting(const wchar_t* valueName, ...) {
    wchar_t formattedName[256];
    va_list args;
    va_start(args, valueName);
    _vsnwprintf_s(formattedName, 256, _TRUNCATE, valueName, args);
    va_end(args);

    wchar_t iniPath[MAX_PATH];
    Wendo_GetSettingsIniPath(iniPath, MAX_PATH);
    return GetPrivateProfileIntW(L"Settings", formattedName, 0, iniPath);
}

static inline const wchar_t* Wh_GetStringSetting(const wchar_t* valueName, ...) {
    wchar_t formattedName[256];
    va_list args;
    va_start(args, valueName);
    _vsnwprintf_s(formattedName, 256, _TRUNCATE, valueName, args);
    va_end(args);

    wchar_t iniPath[MAX_PATH];
    Wendo_GetSettingsIniPath(iniPath, MAX_PATH);
    wchar_t* buf = (wchar_t*)malloc(1024 * sizeof(wchar_t));
    if (!buf) return L"";
    GetPrivateProfileStringW(L"Settings", formattedName, L"", buf, 1024, iniPath);
    return buf;
}

static inline void Wh_FreeStringSetting(const wchar_t* string) {
    if (string) free((void*)string);
}

static inline void Wh_SetIntSetting(const wchar_t* valueName, int value) {
    wchar_t iniPath[MAX_PATH];
    Wendo_GetSettingsIniPath(iniPath, MAX_PATH);
    wchar_t buf[32];
    _snwprintf_s(buf, 32, _TRUNCATE, L"%d", value);
    WritePrivateProfileStringW(L"Settings", valueName, buf, iniPath);
}

static inline void Wh_SetStringSetting(const wchar_t* valueName, const wchar_t* value) {
    wchar_t iniPath[MAX_PATH];
    Wendo_GetSettingsIniPath(iniPath, MAX_PATH);
    WritePrivateProfileStringW(L"Settings", valueName, value ? value : L"", iniPath);
}

static inline int Wh_GetIntValue(const wchar_t* valueName, int defaultValue) {
    wchar_t iniPath[MAX_PATH];
    Wendo_GetSettingsIniPath(iniPath, MAX_PATH);
    return GetPrivateProfileIntW(L"Values", valueName, defaultValue, iniPath);
}

static inline void Wh_SetIntValue(const wchar_t* valueName, int value) {
    wchar_t iniPath[MAX_PATH];
    Wendo_GetSettingsIniPath(iniPath, MAX_PATH);
    wchar_t buf[32];
    _snwprintf_s(buf, 32, _TRUNCATE, L"%d", value);
    WritePrivateProfileStringW(L"Values", valueName, buf, iniPath);
}

static inline size_t Wh_GetStringValue(const wchar_t* valueName, wchar_t* stringBuffer, size_t bufferChars) {
    if (!stringBuffer || bufferChars == 0) return 0;
    wchar_t iniPath[MAX_PATH];
    Wendo_GetSettingsIniPath(iniPath, MAX_PATH);
    return GetPrivateProfileStringW(L"Values", valueName, L"", stringBuffer, (DWORD)bufferChars, iniPath);
}

static inline void Wh_SetStringValue(const wchar_t* valueName, const wchar_t* value) {
    wchar_t iniPath[MAX_PATH];
    Wendo_GetSettingsIniPath(iniPath, MAX_PATH);
    WritePrivateProfileStringW(L"Values", valueName, value ? value : L"", iniPath);
}

// Mod lifecycle callbacks implemented by the mod
BOOL Wh_ModInit(void);
void Wh_ModUninit(void);
void Wh_ModSettingsChanged(void);

#ifdef __cplusplus
}
#endif

// Standard DllMain entry point for Omnilynx mods (initializes outside OS Loader Lock)
#ifndef WENDO_NO_DLLMAIN
static DWORD WINAPI WendoInitThreadProc(LPVOID) {
    if (Wh_ModInit()) {
        Wh_ApplyHookOperations();
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            HANDLE hThread = CreateThread(nullptr, 0, WendoInitThreadProc, nullptr, 0, nullptr);
            if (hThread) CloseHandle(hThread);
            break;
        }
        case DLL_PROCESS_DETACH:
            Wh_RemoveAllHooks();
            Wh_ModUninit();
            break;
    }
    return TRUE;
}
#endif
