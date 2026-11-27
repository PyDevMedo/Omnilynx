#pragma once
#include "windhawk_api.h"
#include <initializer_list>
#include <utility>
#include <vector>

namespace WindhawkUtils {

class StringSetting {
public:
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
        reinterpret_cast<void**>(originalFunction));
}

template <typename T>
inline bool Wh_SetFunctionHookT(T targetFunction, T hookFunction, T* originalFunction) {
    return Wh_SetFunctionHook(
        reinterpret_cast<void*>(targetFunction),
        reinterpret_cast<void*>(hookFunction),
        reinterpret_cast<void**>(originalFunction));
}

struct SYMBOL_HOOK {
    std::vector<PCWSTR> symbols;
    void** pOriginalFunction = nullptr;
    void* hookFunction = nullptr;
    bool optional = false;

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

} // namespace WindhawkUtils
