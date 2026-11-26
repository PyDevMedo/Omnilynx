// ==WindhawkMod==
// @id              omnilynx-rounded-corners
// @name            Windows 11 Rounded Corners
// @description     Enforces crisp Windows 11 rounded window corners on target applications
// @version         1.0.0
// @author          Omnilynx
// @include         notepad.exe
// @compilerOptions -luser32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Windows 11 Rounded Corners
Applies `DWMWA_WINDOW_CORNER_PREFERENCE` (`DWMWCP_ROUND` or `DWMWCP_ROUNDSMALL`) to target windows.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- cornerStyle: 2
  $name: Corner Style (2=Round, 3=Small Round, 1=Square)
*/
// ==/WindhawkModSettings==

#include "wendo_mod.h"
#include <dwmapi.h>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

static BOOL CALLBACK ApplyCornersProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
        int pref = (int)lParam;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
    }
    return TRUE;
}

BOOL Wh_ModInit() {
    int pref = Wh_GetIntSetting(L"cornerStyle");
    if (pref <= 0) pref = 2;
    EnumWindows(ApplyCornersProc, pref);
    return TRUE;
}

void Wh_ModUninit() {
    EnumWindows(ApplyCornersProc, 0);
}

void Wh_ModSettingsChanged() {
    int pref = Wh_GetIntSetting(L"cornerStyle");
    if (pref <= 0) pref = 2;
    EnumWindows(ApplyCornersProc, pref);
}
