// ==WindhawkMod==
// @id              win-taskbar-floating-style
// @name            Taskbar Translucent & Rounded Style
// @description     Applies Windows 11 rounded corners, border accent, and translucent backdrop to the Taskbar
// @version         1.0.0
// @author          Omnilynx
// @include         explorer.exe
// @compilerOptions -luser32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Taskbar Translucent & Rounded Style
Styles `Shell_TrayWnd` with Windows 11 rounded corners and an Acrylic/Mica backdrop via native DWM APIs.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- backdropStyle: 3
  $name: Taskbar Backdrop (2=Mica, 3=Acrylic, 4=Tabbed)
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"
#include <dwmapi.h>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

namespace {

void StyleTaskbar(int backdrop, int corner) noexcept {
    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (tray) {
        (void)DwmSetWindowAttribute(tray, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        (void)DwmSetWindowAttribute(tray, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    }
}

class TaskbarFloatingStyleMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-taskbar-floating-style"; }
    const wchar_t* Name() const noexcept override { return L"Taskbar Translucent & Rounded Style"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        int style = Wh_GetIntSetting(L"backdropStyle");
        if (style < 2 || style > 4) style = 3;
        StyleTaskbar(style, 2);
        return true;
    }

    void OnUninit() noexcept override {
        StyleTaskbar(1, 0);
    }

    void OnSettingsChanged() override {
        int style = Wh_GetIntSetting(L"backdropStyle");
        if (style < 2 || style > 4) style = 3;
        StyleTaskbar(style, 2);
    }
};

} // namespace

OMNILYNX_REGISTER_MOD(TaskbarFloatingStyleMod)
