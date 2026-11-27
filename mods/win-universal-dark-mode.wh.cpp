// ==WindhawkMod==
// @id              win-universal-dark-mode
// @name            Universal Dark Titlebar & Border
// @description     Enforces Windows 11/10 Immersive Dark Mode and sleek dark border on target windows
// @version         1.0.0
// @author          Omnilynx
// @include         notepad.exe
// @compilerOptions -luser32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Universal Dark Titlebar & Border
Applies `DWMWA_USE_IMMERSIVE_DARK_MODE` and a clean dark accent border to all visible windows in the target process.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- darkMode: true
  $name: Enable Dark Titlebars & Borders
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

namespace {

BOOL CALLBACK ApplyDarkProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
        const BOOL dark = (lParam != 0) ? TRUE : FALSE;
        const COLORREF border = dark ? RGB(30, 41, 59) : 0xFFFFFFFFU;
        (void)DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        (void)DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
    }
    return TRUE;
}

class UniversalDarkModeMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-universal-dark-mode"; }
    const wchar_t* Name() const noexcept override { return L"Universal Dark Titlebar & Border"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        EnumWindows(ApplyDarkProc, TRUE);
        return true;
    }

    void OnUninit() noexcept override {
        EnumWindows(ApplyDarkProc, FALSE);
    }

    void OnSettingsChanged() override {
        const BOOL dark = (Wh_GetIntSetting(L"darkMode") != 0) ? TRUE : FALSE;
        EnumWindows(ApplyDarkProc, static_cast<LPARAM>(dark));
    }
};

} // namespace

OMNILYNX_REGISTER_MOD(UniversalDarkModeMod)
