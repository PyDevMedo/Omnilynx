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

#ifdef __GNUC__
__attribute__((weak)) void Wh_ModUninit(void);
__attribute__((weak)) void Wh_ModBeforeInit(void);
__attribute__((weak)) void Wh_ModAfterInit(void);
__attribute__((weak)) void Wh_ModSettingsChanged(void);
#else
void Wh_ModUninit(void);
void Wh_ModSettingsChanged(void);
#endif

// ============================================================================
// Native DbgHelp PDB Symbol Resolution (Wh_FindFirstSymbol / Wh_HookSymbols)
// ============================================================================
typedef struct WH_FIND_SYMBOL_OPTIONS {
    size_t optionsSize;
    BOOL noUnmangle;
} WH_FIND_SYMBOL_OPTIONS;

typedef struct WH_FIND_SYMBOL {
    void* address;
    const wchar_t* symbol;
    const wchar_t* symbolDecorated;
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
    const wchar_t* symbolServer;
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
using SymCleanupFn = BOOL(WINAPI*)(HANDLE);
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

static inline bool EnsureSymInitialized() noexcept {
    static volatile LONG s_init = 0;
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

        wchar_t symCache[1024] = {};
        ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Omnilynx\\symbols", symCache, 1024);
        CreateDirectoryW(symCache, nullptr);

        wchar_t symPath[1280] = {};
        _snwprintf_s(symPath, 1280, _TRUNCATE, L"srv*%s*https://msdl.microsoft.com/download/symbols", symCache);
        if (!symInitW(GetCurrentProcess(), symPath, FALSE)) {
            InterlockedExchange(&s_init, 0);
            return false;
        }
    }
    return true;
}

static inline DWORD64 LoadModuleSymbols(HMODULE hModule) noexcept {
    if (!EnsureSymInitialized()) return 0;
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
    const WH_FIND_SYMBOL_OPTIONS* /*options*/,
    WH_FIND_SYMBOL* findData)
{
    if (!hModule || !findData) return nullptr;
    DWORD64 modBase = omnilynx_pdb::LoadModuleSymbols(hModule);
    if (!modBase) return nullptr;

    HMODULE hDbg = omnilynx_pdb::GetDbgHelp();
    auto symEnum = reinterpret_cast<omnilynx_pdb::SymEnumSymbolsWFn>(
        reinterpret_cast<void*>(GetProcAddress(hDbg, "SymEnumSymbolsW")));
    if (!symEnum) return nullptr;

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

    // 1. Fast path: check OS-build RVA cache first (0ms network delay)
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

    // 2. If any non-cached symbols remain, enumerate via DbgHelp
    if (resolvedCount < symbolHooksCount) {
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

    // 3. Apply hooks via MinHook
    bool allRequiredOk = true;
    for (size_t i = 0; i < symbolHooksCount; ++i) {
        if (!resolvedAddrs[i]) {
            if (!symbolHooks[i].optional) allRequiredOk = false;
            continue;
        }
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
