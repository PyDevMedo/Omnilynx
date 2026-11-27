// ==WindhawkMod==
// @id              win-taskbar-autohide-toggle
// @name            Taskbar Auto-Hide Controller
// @description     Cleanly toggles Windows Taskbar auto-hide state via native SHAppBarMessage API
// @version         1.0.0
// @author          Omnilynx
// @include         explorer.exe
// @compilerOptions -luser32 -lshell32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Taskbar Auto-Hide Controller
Uses native `SHAppBarMessage(ABM_SETSTATE, ...)` to toggle Taskbar auto-hide cleanly and restores your original Taskbar state on unload.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- autoHide: true
  $name: Auto-Hide Windows Taskbar
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"
#include <shellapi.h>

namespace {

class TaskbarAutoHideMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-taskbar-autohide-toggle"; }
    const wchar_t* Name() const noexcept override { return L"Taskbar Auto-Hide Controller"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        APPBARDATA abd{};
        abd.cbSize = sizeof(abd);
        m_prevState = static_cast<UINT>(SHAppBarMessage(ABM_GETSTATE, &abd));
        SetAutoHide(true);
        return true;
    }

    void OnUninit() noexcept override {
        APPBARDATA abd{};
        abd.cbSize = sizeof(abd);
        abd.lParam = static_cast<LPARAM>(m_prevState);
        SHAppBarMessage(ABM_SETSTATE, &abd);
    }

    void OnSettingsChanged() override {
        SetAutoHide(Wh_GetIntSetting(L"autoHide") != 0);
    }

private:
    static void SetAutoHide(bool enable) noexcept {
        APPBARDATA abd{};
        abd.cbSize = sizeof(abd);
        abd.lParam = enable ? ABS_AUTOHIDE : 0;
        SHAppBarMessage(ABM_SETSTATE, &abd);
    }

    UINT m_prevState = 0;
};

} // namespace

OMNILYNX_REGISTER_MOD(TaskbarAutoHideMod)
