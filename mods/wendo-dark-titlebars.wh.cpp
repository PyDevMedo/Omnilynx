// ==WindhawkMod==
// @id              wendo-dark-titlebars
// @name            Force Dark Titlebars
// @description     Applies Windows 11 Immersive Dark Mode to all top-level windows in the target process
// @version         1.0.0
// @author          Wendo Studio
// @include         notepad.exe
// @compilerOptions -luser32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Force Dark Titlebars
Automatically enables **Immersive Dark Mode** (`DWMWA_USE_IMMERSIVE_DARK_MODE`) on top-level windows inside the target process.
Works standalone with Wendo Engine — no external dependencies required.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- darkMode: true
  $name: Enable Dark Titlebars
  $description: Force dark window borders and titlebars
*/
// ==/WindhawkModSettings==

#include "wendo_mod.h"
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

static BOOL CALLBACK ApplyDarkTitlebarProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
        BOOL useDark = (BOOL)lParam;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
    }
    return TRUE;
}

BOOL Wh_ModInit() {
    Wh_Log(L"Force Dark Titlebars initialized!");
    EnumWindows(ApplyDarkTitlebarProc, TRUE);
    return TRUE;
}

void Wh_ModUninit() {
    EnumWindows(ApplyDarkTitlebarProc, FALSE);
    Wh_Log(L"Force Dark Titlebars unloaded!");
}

void Wh_ModSettingsChanged() {
    BOOL dark = Wh_GetIntSetting(L"darkMode") != 0;
    EnumWindows(ApplyDarkTitlebarProc, dark);
}
