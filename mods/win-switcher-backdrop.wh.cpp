// ==WindhawkMod==
// @id              win-switcher-backdrop
// @name            Windows Switcher & Explorer Backdrop
// @description     Applies dark mode, rounded corners, and translucent backdrop styling to Explorer windows
// @version         1.0.0
// @author          Omnilynx
// @include         explorer.exe
// @compilerOptions -luser32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Windows Switcher & Explorer Backdrop
Enhances Windows Explorer and switcher surfaces with Windows 11 DWM rounded corners, dark borders, and Mica/Acrylic backdrop.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- backdropType: 2
  $name: Backdrop Style (2=Mica, 3=Acrylic, 4=Tabbed)
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

namespace {

BOOL CALLBACK StyleExplorerProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
        const int type = static_cast<int>(lParam);
        const BOOL dark = (type > 1) ? TRUE : FALSE;
        const int corner = (type > 1) ? 2 : 0;
        (void)DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        (void)DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        (void)DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
    }
    return TRUE;
}

class SwitcherBackdropMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-switcher-backdrop"; }
    const wchar_t* Name() const noexcept override { return L"Windows Switcher & Explorer Backdrop"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        int type = Wh_GetIntSetting(L"backdropType");
        if (type < 2 || type > 4) type = 2;
        EnumWindows(StyleExplorerProc, static_cast<LPARAM>(type));
        return true;
    }

    void OnUninit() noexcept override {
        EnumWindows(StyleExplorerProc, 1);
    }

    void OnSettingsChanged() override {
        int type = Wh_GetIntSetting(L"backdropType");
        if (type < 2 || type > 4) type = 2;
        EnumWindows(StyleExplorerProc, static_cast<LPARAM>(type));
    }
};

} // namespace

OMNILYNX_REGISTER_MOD(SwitcherBackdropMod)
