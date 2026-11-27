// ==WindhawkMod==
// @id              wendo-window-opacity
// @name            Glass Window Opacity
// @description     Makes target application windows semi-transparent with configurable opacity
// @version         1.0.0
// @author          Wendo Studio
// @include         notepad.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Glass Window Opacity
Sets a clean semi-transparent glass opacity on all visible windows in the target process.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- opacity: 225
  $name: Window Opacity (50-255)
  $description: 255 is fully opaque, 200-230 gives a sleek glass look
*/
// ==/WindhawkModSettings==

#include "wendo_mod.h"

static BOOL CALLBACK ApplyOpacityProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
        BYTE alpha = (BYTE)lParam;
        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if (alpha < 255) {
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
            SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
        } else {
            SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
        }
    }
    return TRUE;
}

BOOL Wh_ModInit() {
    int alpha = Wh_GetIntSetting(L"opacity");
    if (alpha < 50 || alpha > 255) alpha = 225;
    EnumWindows(ApplyOpacityProc, alpha);
    return TRUE;
}

void Wh_ModUninit() {
    EnumWindows(ApplyOpacityProc, 255);
}

void Wh_ModSettingsChanged() {
    int alpha = Wh_GetIntSetting(L"opacity");
    if (alpha < 50 || alpha > 255) alpha = 225;
    EnumWindows(ApplyOpacityProc, alpha);
}
