#pragma once
#include "windhawk_api.h"
#include <utility>

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
inline bool SetFunctionHook(void* targetFunction, T hookFunction, T* originalFunction) {
    return Wh_SetFunctionHook(
        targetFunction,
        reinterpret_cast<void*>(hookFunction),
        reinterpret_cast<void**>(originalFunction));
}

template <typename T>
inline bool Wh_SetFunctionHookT(void* targetFunction, T hookFunction, T* originalFunction) {
    return Wh_SetFunctionHook(
        targetFunction,
        reinterpret_cast<void*>(hookFunction),
        reinterpret_cast<void**>(originalFunction));
}

} // namespace WindhawkUtils
