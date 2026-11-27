// ==WindhawkMod==
// @id              win-alttab-focus-guard
// @name            Windows Alt+Tab Focus Guard
// @description     Reduces accidental Alt+Tab and foreground focus-stealing flicker across Windows 10/11
// @version         1.0.0
// @author          Omnilynx
// @include         explorer.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Windows Alt+Tab Focus Guard
Adjusts the Windows foreground lock timeout (`SPI_SETFOREGROUNDLOCKTIMEOUT`) to prevent rapid focus-stealing flicker during fast Alt+Tab switching, and restores your original setting on unload.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- lockTimeoutMs: 200
  $name: Focus Guard Timeout (ms)
  $description: Delay in milliseconds before background apps can steal focus (0-2000)
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"

namespace {

class AltTabFocusGuardMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-alttab-focus-guard"; }
    const wchar_t* Name() const noexcept override { return L"Windows Alt+Tab Focus Guard"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        SystemParametersInfoW(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &m_prevTimeout, 0);
        ApplySetting();
        return true;
    }

    void OnUninit() noexcept override {
        SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(m_prevTimeout)), SPIF_SENDCHANGE);
    }

    void OnSettingsChanged() override {
        ApplySetting();
    }

private:
    void ApplySetting() {
        int val = Wh_GetIntSetting(L"lockTimeoutMs");
        if (val < 0 || val > 2000) val = 200;
        SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(val)), SPIF_SENDCHANGE);
    }

    DWORD m_prevTimeout = 0;
};

} // namespace

OMNILYNX_REGISTER_MOD(AltTabFocusGuardMod)
