// ==WindhawkMod==
// @id              win-mica-acrylic-backdrop
// @name            Mica & Acrylic Window Backdrop
// @description     Applies Windows 11 Mica / Acrylic / Tabbed backdrop to target application windows
// @version         1.0.0
// @author          Omnilynx
// @include         notepad.exe
// @compilerOptions -luser32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Mica & Acrylic Window Backdrop
Applies Windows 11 native **Mica / Acrylic** (`DWMWA_SYSTEMBACKDROP_TYPE`) material to windows.
- `2` = Mica
- `3` = Acrylic
- `4` = Tabbed Mica
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- backdropType: 3
  $name: Backdrop Style (2=Mica, 3=Acrylic, 4=Tabbed)
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"
#include <dwmapi.h>

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

namespace {

BOOL CALLBACK ApplyBackdropProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
        int type = static_cast<int>(lParam);
        (void)DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
    }
    return TRUE;
}

class MicaAcrylicBackdropMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-mica-acrylic-backdrop"; }
    const wchar_t* Name() const noexcept override { return L"Mica & Acrylic Window Backdrop"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        int type = Wh_GetIntSetting(L"backdropType");
        if (type < 2 || type > 4) type = 3;
        EnumWindows(ApplyBackdropProc, static_cast<LPARAM>(type));
        return true;
    }

    void OnUninit() noexcept override {
        EnumWindows(ApplyBackdropProc, 1);
    }

    void OnSettingsChanged() override {
        int type = Wh_GetIntSetting(L"backdropType");
        if (type < 2 || type > 4) type = 3;
        EnumWindows(ApplyBackdropProc, static_cast<LPARAM>(type));
    }
};

} // namespace

OMNILYNX_REGISTER_MOD(MicaAcrylicBackdropMod)
