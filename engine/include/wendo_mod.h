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

#ifdef __cplusplus
extern "C" {
#endif

// Logging helper (writes to OutputDebugStringW and optional log file)
static inline void Wh_Log(const wchar_t* format, ...) {
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, 1024, _TRUNCATE, format, args);
    va_end(args);
    OutputDebugStringW(L"[WendoMod] ");
    OutputDebugStringW(buffer);
    OutputDebugStringW(L"\n");
}

// Lightweight inline x64/x86 hook entry
struct WendoHookEntry {
    void* targetFunction;
    void* hookFunction;
    void** originalTrampoline;
    uint8_t originalBytes[16];
    size_t patchSize;
    bool active;
};

static WendoHookEntry g_wendoHooks[64];
static int g_wendoHookCount = 0;

static inline bool Wh_SetFunctionHook(
    void* targetFunction,
    void* hookFunction,
    void** originalFunction
) {
    if (!targetFunction || !hookFunction || g_wendoHookCount >= 64) {
        return false;
    }

    WendoHookEntry& entry = g_wendoHooks[g_wendoHookCount];
    entry.targetFunction = targetFunction;
    entry.hookFunction = hookFunction;
    entry.originalTrampoline = originalFunction;
    entry.active = false;

#if defined(_M_X64) || defined(__x86_64__)
    // 14-byte absolute jump: FF 25 00 00 00 00 [8-byte address]
    entry.patchSize = 14;
    memcpy(entry.originalBytes, targetFunction, entry.patchSize);

    // Allocate executable trampoline: original 14 bytes + 14-byte jump back
    uint8_t* trampoline = (uint8_t*)VirtualAlloc(
        nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE
    );
    if (!trampoline) return false;

    memcpy(trampoline, entry.originalBytes, entry.patchSize);
    uint8_t* jumpBack = trampoline + entry.patchSize;
    jumpBack[0] = 0xFF;
    jumpBack[1] = 0x25;
    *(uint32_t*)(jumpBack + 2) = 0;
    *(uint64_t*)(jumpBack + 6) = (uint64_t)((uint8_t*)targetFunction + entry.patchSize);

    if (originalFunction) {
        *originalFunction = (void*)trampoline;
    }
    g_wendoHookCount++;
    return true;
#else
    return false;
#endif
}

static inline bool Wh_ApplyHookOperations(void) {
    for (int i = 0; i < g_wendoHookCount; i++) {
        WendoHookEntry& entry = g_wendoHooks[i];
        if (entry.active) continue;

        DWORD oldProtect = 0;
        if (!VirtualProtect(entry.targetFunction, entry.patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            return false;
        }

        uint8_t patch[14];
        patch[0] = 0xFF;
        patch[1] = 0x25;
        *(uint32_t*)(patch + 2) = 0;
        *(uint64_t*)(patch + 6) = (uint64_t)entry.hookFunction;

        memcpy(entry.targetFunction, patch, 14);
        VirtualProtect(entry.targetFunction, entry.patchSize, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), entry.targetFunction, 14);
        entry.active = true;
    }
    return true;
}

static inline void Wh_RemoveAllHooks(void) {
    for (int i = 0; i < g_wendoHookCount; i++) {
        WendoHookEntry& entry = g_wendoHooks[i];
        if (!entry.active) continue;

        DWORD oldProtect = 0;
        if (VirtualProtect(entry.targetFunction, entry.patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(entry.targetFunction, entry.originalBytes, entry.patchSize);
            VirtualProtect(entry.targetFunction, entry.patchSize, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), entry.targetFunction, entry.patchSize);
            entry.active = false;
        }
        if (entry.originalTrampoline && *entry.originalTrampoline) {
            VirtualFree(*entry.originalTrampoline, 0, MEM_RELEASE);
            *entry.originalTrampoline = nullptr;
        }
    }
    g_wendoHookCount = 0;
}

// Settings helpers (reads from %PROGRAMDATA%\Wendo\settings\<mod>.ini or defaults)
static inline int Wh_GetIntSetting(const wchar_t* valueName, ...) {
    wchar_t iniPath[MAX_PATH];
    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Wendo\\mod_settings.ini", iniPath, MAX_PATH);
    return GetPrivateProfileIntW(L"Settings", valueName, 0, iniPath);
}

static inline const wchar_t* Wh_GetStringSetting(const wchar_t* valueName, ...) {
    wchar_t iniPath[MAX_PATH];
    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Wendo\\mod_settings.ini", iniPath, MAX_PATH);
    wchar_t* buf = (wchar_t*)malloc(1024 * sizeof(wchar_t));
    if (!buf) return L"";
    GetPrivateProfileStringW(L"Settings", valueName, L"", buf, 1024, iniPath);
    return buf;
}

static inline void Wh_FreeStringSetting(const wchar_t* string) {
    if (string) free((void*)string);
}

// Mod lifecycle callbacks implemented by the mod
BOOL Wh_ModInit(void);
void Wh_ModUninit(void);
void Wh_ModSettingsChanged(void);

#ifdef __cplusplus
}
#endif

// Standard DllMain entry point for Wendo mods
#ifndef WENDO_NO_DLLMAIN
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            if (!Wh_ModInit()) {
                return FALSE;
            }
            Wh_ApplyHookOperations();
            break;
        case DLL_PROCESS_DETACH:
            Wh_RemoveAllHooks();
            Wh_ModUninit();
            break;
    }
    return TRUE;
}
#endif
