// ==WindhawkMod==
// @id              omnilynx-border-accent
// @name            Glowing Border Accent
// @description     Applies a custom glowing accent border color to Windows 11 windows
// @version         1.0.0
// @author          Omnilynx
// @include         notepad.exe
// @compilerOptions -luser32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Glowing Border Accent
Sets a custom `DWMWA_BORDER_COLOR` on Windows 11 windows for a sleek neon or gold outline.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- accentPreset: 1
  $name: Border Color Preset (1=Cyan, 2=Emerald, 3=Indigo)
*/
// ==/WindhawkModSettings==

#include "wendo_mod.h"
#include <dwmapi.h>

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

static COLORREF GetPresetColor(int preset) {
    if (preset == 2) return RGB(16, 185, 129);  // Emerald
    if (preset == 3) return RGB(99, 102, 241);  // Indigo
    return RGB(56, 189, 248);                   // Cyan (Omnilynx)
}

static BOOL CALLBACK ApplyBorderProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
        COLORREF color = (COLORREF)lParam;
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &color, sizeof(color));
    }
    return TRUE;
}

BOOL Wh_ModInit() {
    int preset = Wh_GetIntSetting(L"accentPreset");
    EnumWindows(ApplyBorderProc, (LPARAM)GetPresetColor(preset));
    return TRUE;
}

void Wh_ModUninit() {
    EnumWindows(ApplyBorderProc, (LPARAM)0xFFFFFFFF); // DWMWA_COLOR_DEFAULT
}

void Wh_ModSettingsChanged() {
    int preset = Wh_GetIntSetting(L"accentPreset");
    EnumWindows(ApplyBorderProc, (LPARAM)GetPresetColor(preset));
}
