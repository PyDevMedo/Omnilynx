// ==WindhawkMod==
// @id              win-smooth-window-transitions
// @name            Smooth DWM Window Transitions
// @description     Enables smooth DWM minimize/restore transitions and rounded corners on target windows
// @version         1.0.0
// @author          Omnilynx
// @include         notepad.exe
// @compilerOptions -luser32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Smooth DWM Window Transitions
Ensures DWM window transitions (`DWMWA_TRANSITIONS_FORCEDISABLED = FALSE`) and crisp Windows 11 rounded corners are active on target windows.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- smoothTransitions: true
  $name: Enable Smooth DWM Transitions
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"
#include <dwmapi.h>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

namespace {

BOOL CALLBACK ApplyTransitionsProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
        const BOOL enable = (lParam != 0) ? TRUE : FALSE;
        const BOOL forceDisabled = enable ? FALSE : TRUE;
        const int corner = enable ? 2 : 0;
        (void)DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &forceDisabled, sizeof(forceDisabled));
        (void)DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    }
    return TRUE;
}

class SmoothTransitionsMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-smooth-window-transitions"; }
    const wchar_t* Name() const noexcept override { return L"Smooth DWM Window Transitions"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        EnumWindows(ApplyTransitionsProc, TRUE);
        return true;
    }

    void OnUninit() noexcept override {
        EnumWindows(ApplyTransitionsProc, FALSE);
    }

    void OnSettingsChanged() override {
        const BOOL enable = (Wh_GetIntSetting(L"smoothTransitions") != 0) ? TRUE : FALSE;
        EnumWindows(ApplyTransitionsProc, static_cast<LPARAM>(enable));
    }
};

} // namespace

OMNILYNX_REGISTER_MOD(SmoothTransitionsMod)
