// ==WindhawkMod==
// @id              wendo-translucent-mica
// @name            Translucent Mica Backdrop
// @description     Applies Windows 11 Mica / Acrylic backdrop to windows in the target process
// @version         1.0.0
// @author          Wendo Studio
// @include         notepad.exe
// @compilerOptions -luser32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Translucent Mica Backdrop
Applies Windows 11's native **Mica / Acrylic** (`DWMWA_SYSTEMBACKDROP_TYPE`) material to windows.
- `2` = Mica
- `3` = Acrylic
- `4` = Tabbed Mica
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- backdropType: 3
  $name: Backdrop Style (2=Mica, 3=Acrylic, 4=Tabbed)
  $description: Choose the Windows 11 DWM backdrop material
*/
// ==/WindhawkModSettings==

#include "wendo_mod.h"
#include <dwmapi.h>

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

static BOOL CALLBACK ApplyBackdropProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
        int type = (int)lParam;
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
    }
    return TRUE;
}

BOOL Wh_ModInit() {
    int type = Wh_GetIntSetting(L"backdropType");
    if (type <= 0) type = 3; // Default Acrylic
    EnumWindows(ApplyBackdropProc, type);
    Wh_Log(L"Translucent Mica Backdrop initialized!");
    return TRUE;
}

void Wh_ModUninit() {
    EnumWindows(ApplyBackdropProc, 1); // Reset to None
    Wh_Log(L"Translucent Mica Backdrop unloaded!");
}

void Wh_ModSettingsChanged() {
    int type = Wh_GetIntSetting(L"backdropType");
    if (type <= 0) type = 3;
    EnumWindows(ApplyBackdropProc, type);
}
