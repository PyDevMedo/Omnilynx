#pragma once
#include "windhawk_api.h"
#include <commctrl.h>
#include <initializer_list>
#include <mutex>
#include <utility>
#include <vector>

namespace WindhawkUtils {

using WH_HOOK_SYMBOL_OPTIONS = ::WH_HOOK_SYMBOLS_OPTIONS;
using WH_HOOK_SYMBOLS_OPTIONS = ::WH_HOOK_SYMBOLS_OPTIONS;
using WH_SUBCLASSPROC = LRESULT(CALLBACK*)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, DWORD_PTR dwRefData);

class StringSetting {
public:
    StringSetting() noexcept = default;
    explicit StringSetting(PCWSTR value) noexcept : m_value(value) {}
    ~StringSetting() {
        if (m_value) {
            Wh_FreeStringSetting(m_value);
        }
    }
    StringSetting(const StringSetting&) = delete;
    StringSetting& operator=(const StringSetting&) = delete;
    StringSetting(StringSetting&& other) noexcept : m_value(other.m_value) {
        other.m_value = nullptr;
    }
    StringSetting& operator=(StringSetting&& other) noexcept {
        if (this != &other) {
            if (m_value) Wh_FreeStringSetting(m_value);
            m_value = other.m_value;
            other.m_value = nullptr;
        }
        return *this;
    }

    template <typename... Args>
    static StringSetting make(PCWSTR valueName, Args... args) {
        return StringSetting(Wh_GetStringSetting(valueName, args...));
    }

    PCWSTR get() const noexcept { return m_value ? m_value : L""; }
    operator PCWSTR() const noexcept { return get(); }

private:
    PCWSTR m_value = nullptr;
};

template <typename T>
inline bool SetFunctionHook(T targetFunction, T hookFunction, T* originalFunction) {
    return Wh_SetFunctionHook(
        reinterpret_cast<void*>(targetFunction),
        reinterpret_cast<void*>(hookFunction),
        reinterpret_cast<void**>(originalFunction)) != FALSE;
}

template <typename T>
inline bool Wh_SetFunctionHookT(T targetFunction, T hookFunction, T* originalFunction) {
    return Wh_SetFunctionHook(
        reinterpret_cast<void*>(targetFunction),
        reinterpret_cast<void*>(hookFunction),
        reinterpret_cast<void**>(originalFunction)) != FALSE;
}

struct SYMBOL_HOOK {
    std::vector<PCWSTR> symbols;
    void** pOriginalFunction = nullptr;
    void* hookFunction = nullptr;
    bool optional = false;

    SYMBOL_HOOK() = default;

    template <typename OrigT>
    SYMBOL_HOOK(std::initializer_list<PCWSTR> syms, OrigT* pOrig, bool opt = false)
        : symbols(syms),
          pOriginalFunction(reinterpret_cast<void**>(pOrig)),
          hookFunction(nullptr),
          optional(opt) {}

    template <typename OrigT, typename HookT>
    SYMBOL_HOOK(std::initializer_list<PCWSTR> syms, OrigT* pOrig, HookT hook, bool opt = false)
        : symbols(syms),
          pOriginalFunction(reinterpret_cast<void**>(pOrig)),
          hookFunction(reinterpret_cast<void*>(hook)),
          optional(opt) {}

    template <typename OrigT>
    SYMBOL_HOOK(std::initializer_list<PCWSTR> syms, OrigT* pOrig, std::nullptr_t, bool opt = false)
        : symbols(syms),
          pOriginalFunction(reinterpret_cast<void**>(pOrig)),
          hookFunction(nullptr),
          optional(opt) {}
};

inline bool HookSymbols(
    HMODULE module,
    const SYMBOL_HOOK* symbolHooks,
    size_t symbolHooksCount,
    const WH_HOOK_SYMBOLS_OPTIONS* options = nullptr)
{
    std::vector<WH_SYMBOL_HOOK> rawHooks;
    rawHooks.reserve(symbolHooksCount);
    for (size_t i = 0; i < symbolHooksCount; ++i) {
        const auto& h = symbolHooks[i];
        WH_SYMBOL_HOOK rh{};
        rh.symbols = h.symbols.data();
        rh.symbolsCount = h.symbols.size();
        rh.pOriginalFunction = h.pOriginalFunction;
        rh.hookFunction = h.hookFunction;
        rh.optional = h.optional;
        rawHooks.push_back(rh);
    }
    return Wh_HookSymbols(module, rawHooks.data(), rawHooks.size(), options);
}

inline bool HookSymbols(
    HMODULE module,
    std::initializer_list<SYMBOL_HOOK> symbolHooks,
    const WH_HOOK_SYMBOLS_OPTIONS* options = nullptr)
{
    return HookSymbols(module, symbolHooks.begin(), symbolHooks.size(), options);
}

namespace detail {
struct SubclassEntry {
    HWND hWnd;
    SUBCLASSPROC proc;
};

inline std::mutex& SubclassMutex() {
    static std::mutex m;
    return m;
}

inline std::vector<SubclassEntry>& SubclassList() {
    static std::vector<SubclassEntry> v;
    return v;
}

struct CrossThreadSubclassReq {
    HWND hWnd;
    SUBCLASSPROC proc;
    DWORD_PTR refData;
    bool remove;
    bool done;
    BOOL ok;
};

inline UINT GetSubclassMsg() {
    static UINT msg = RegisterWindowMessageW(L"Omnilynx_WindhawkUtils_SubclassMsg");
    return msg;
}

inline CrossThreadSubclassReq*& ActiveReq() {
    static CrossThreadSubclassReq* req = nullptr;
    return req;
}

inline LRESULT CALLBACK SubclassCallWndProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && lParam) {
        const auto* cwp = reinterpret_cast<const CWPSTRUCT*>(lParam);
        auto* req = ActiveReq();
        if (req && !req->done && cwp->hwnd == req->hWnd && cwp->message == GetSubclassMsg()) {
            if (req->remove) {
                req->ok = RemoveWindowSubclass(req->hWnd, req->proc, static_cast<UINT_PTR>(cwp->wParam));
            } else {
                req->ok = SetWindowSubclass(req->hWnd, req->proc, static_cast<UINT_PTR>(cwp->wParam), req->refData);
            }
            req->done = true;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
} // namespace detail

inline LRESULT CALLBACK WhSubclassThunkProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto pfn = reinterpret_cast<WH_SUBCLASSPROC>(uIdSubclass);
    if (pfn) {
        return pfn(hWnd, uMsg, wParam, lParam, dwRefData);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

inline bool SetWindowSubclassFromAnyThread(HWND hWnd, WH_SUBCLASSPROC pfnSubclass, DWORD_PTR dwRefData) {
    if (!hWnd || !pfnSubclass || !IsWindow(hWnd)) return false;
    DWORD tid = GetWindowThreadProcessId(hWnd, nullptr);
    if (!tid) return false;

    UINT_PTR subId = reinterpret_cast<UINT_PTR>(pfnSubclass);
    BOOL ok = FALSE;
    if (tid == GetCurrentThreadId()) {
        ok = SetWindowSubclass(hWnd, WhSubclassThunkProc, subId, dwRefData);
    } else {
        std::lock_guard<std::mutex> lock(detail::SubclassMutex());
        detail::CrossThreadSubclassReq req{ hWnd, WhSubclassThunkProc, dwRefData, false, false, FALSE };
        detail::ActiveReq() = &req;
        // Pass subId via refData? No, store subId in CrossThreadSubclassReq? Let's just use SetWindowSubclass with subId = 0 if SUBCLASSPROC, or store subId.
        HHOOK hHook = SetWindowsHookExW(WH_CALLWNDPROC, detail::SubclassCallWndProc, nullptr, tid);
        if (hHook) {
            SendMessageW(hWnd, detail::GetSubclassMsg(), static_cast<WPARAM>(subId), 0);
            UnhookWindowsHookEx(hHook);
            ok = req.done ? req.ok : FALSE;
        }
        detail::ActiveReq() = nullptr;
    }

    if (ok) {
        std::lock_guard<std::mutex> lock(detail::SubclassMutex());
        detail::SubclassList().push_back({ hWnd, WhSubclassThunkProc });
    }
    return ok != FALSE;
}

inline bool SetWindowSubclassFromAnyThread(HWND hWnd, SUBCLASSPROC pfnSubclass, DWORD_PTR dwRefData) {
    if (!hWnd || !pfnSubclass || !IsWindow(hWnd)) return false;
    DWORD tid = GetWindowThreadProcessId(hWnd, nullptr);
    if (!tid) return false;

    BOOL ok = FALSE;
    if (tid == GetCurrentThreadId()) {
        ok = SetWindowSubclass(hWnd, pfnSubclass, 0, dwRefData);
    } else {
        std::lock_guard<std::mutex> lock(detail::SubclassMutex());
        detail::CrossThreadSubclassReq req{ hWnd, pfnSubclass, dwRefData, false, false, FALSE };
        detail::ActiveReq() = &req;
        HHOOK hHook = SetWindowsHookExW(WH_CALLWNDPROC, detail::SubclassCallWndProc, nullptr, tid);
        if (hHook) {
            SendMessageW(hWnd, detail::GetSubclassMsg(), 0, 0);
            UnhookWindowsHookEx(hHook);
            ok = req.done ? req.ok : FALSE;
        }
        detail::ActiveReq() = nullptr;
    }

    if (ok) {
        std::lock_guard<std::mutex> lock(detail::SubclassMutex());
        detail::SubclassList().push_back({ hWnd, pfnSubclass });
    }
    return ok != FALSE;
}

inline bool RemoveWindowSubclassFromAnyThread(HWND hWnd, WH_SUBCLASSPROC pfnSubclass) {
    if (!hWnd || !pfnSubclass || !IsWindow(hWnd)) return false;
    DWORD tid = GetWindowThreadProcessId(hWnd, nullptr);
    if (!tid) return false;

    UINT_PTR subId = reinterpret_cast<UINT_PTR>(pfnSubclass);
    BOOL ok = FALSE;
    if (tid == GetCurrentThreadId()) {
        ok = RemoveWindowSubclass(hWnd, WhSubclassThunkProc, subId);
    } else {
        std::lock_guard<std::mutex> lock(detail::SubclassMutex());
        detail::CrossThreadSubclassReq req{ hWnd, WhSubclassThunkProc, 0, true, false, FALSE };
        detail::ActiveReq() = &req;
        HHOOK hHook = SetWindowsHookExW(WH_CALLWNDPROC, detail::SubclassCallWndProc, nullptr, tid);
        if (hHook) {
            SendMessageW(hWnd, detail::GetSubclassMsg(), static_cast<WPARAM>(subId), 0);
            UnhookWindowsHookEx(hHook);
            ok = req.done ? req.ok : FALSE;
        }
        detail::ActiveReq() = nullptr;
    }
    return ok != FALSE;
}

inline bool RemoveWindowSubclassFromAnyThread(HWND hWnd, SUBCLASSPROC pfnSubclass) {
    if (!hWnd || !pfnSubclass || !IsWindow(hWnd)) return false;
    DWORD tid = GetWindowThreadProcessId(hWnd, nullptr);
    if (!tid) return false;

    BOOL ok = FALSE;
    if (tid == GetCurrentThreadId()) {
        ok = RemoveWindowSubclass(hWnd, pfnSubclass, 0);
    } else {
        std::lock_guard<std::mutex> lock(detail::SubclassMutex());
        detail::CrossThreadSubclassReq req{ hWnd, pfnSubclass, 0, true, false, FALSE };
        detail::ActiveReq() = &req;
        HHOOK hHook = SetWindowsHookExW(WH_CALLWNDPROC, detail::SubclassCallWndProc, nullptr, tid);
        if (hHook) {
            SendMessageW(hWnd, detail::GetSubclassMsg(), 0, 0);
            UnhookWindowsHookEx(hHook);
            ok = req.done ? req.ok : FALSE;
        }
        detail::ActiveReq() = nullptr;
    }
    return ok != FALSE;
}

inline void RemoveAllWindowSubclasses() {
    std::vector<detail::SubclassEntry> copy;
    {
        std::lock_guard<std::mutex> lock(detail::SubclassMutex());
        copy.swap(detail::SubclassList());
    }
    for (const auto& e : copy) {
        if (IsWindow(e.hWnd)) {
            RemoveWindowSubclassFromAnyThread(e.hWnd, e.proc);
        }
    }
}

} // namespace WindhawkUtils
