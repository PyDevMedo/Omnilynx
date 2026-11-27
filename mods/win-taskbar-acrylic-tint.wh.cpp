// ==WindhawkMod==
// @id              win-taskbar-acrylic-tint
// @name            Taskbar & Tray Acrylic Tint
// @description     Applies dark mode and acrylic tint to the Windows Taskbar and overflow tray flyout
// @version         1.0.0
// @author          Omnilynx
// @include         explorer.exe
// @compilerOptions -luser32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Taskbar & Tray Acrylic Tint
Applies Windows 11 Immersive Dark Mode and Acrylic backdrop to `Shell_TrayWnd` and `NotifyIconOverflowWindow`.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- darkTint: true
  $name: Enable Dark Acrylic Tint
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

namespace {

void ApplyTrayTint(bool enable) noexcept {
    const BOOL dark = enable ? TRUE : FALSE;
    const int backdrop = enable ? 3 : 1; // 3=Acrylic, 1=None
    const wchar_t* classes[] = { L"Shell_TrayWnd", L"NotifyIconOverflowWindow" };
    for (const wchar_t* cls : classes) {
        HWND hwnd = FindWindowW(cls, nullptr);
        if (hwnd) {
            (void)DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
            (void)DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
        }
    }
}

class TaskbarAcrylicTintMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-taskbar-acrylic-tint"; }
    const wchar_t* Name() const noexcept override { return L"Taskbar & Tray Acrylic Tint"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        ApplyTrayTint(true);
        return true;
    }

    void OnUninit() noexcept override {
        ApplyTrayTint(false);
    }

    void OnSettingsChanged() override {
        ApplyTrayTint(Wh_GetIntSetting(L"darkTint") != 0);
    }
};

} // namespace

OMNILYNX_REGISTER_MOD(TaskbarAcrylicTintMod)
