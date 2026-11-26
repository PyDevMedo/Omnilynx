// ==WindhawkMod==
// @id              omnilynx-always-on-top
// @name            Always On Top Pin
// @description     Pins target application windows to stay always on top of other windows
// @version         1.0.0
// @author          Omnilynx
// @include         notepad.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Always On Top Pin
Keeps target application windows pinned above all other windows (`HWND_TOPMOST`).
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- topmost: true
  $name: Keep Window On Top
  $description: Pin or unpin the target window
*/
// ==/WindhawkModSettings==

#include "wendo_mod.h"

static BOOL CALLBACK PinWindowProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
        HWND insertAfter = lParam ? HWND_TOPMOST : HWND_NOTOPMOST;
        SetWindowPos(hwnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    return TRUE;
}

BOOL Wh_ModInit() {
    EnumWindows(PinWindowProc, TRUE);
    return TRUE;
}

void Wh_ModUninit() {
    EnumWindows(PinWindowProc, FALSE);
}

void Wh_ModSettingsChanged() {
    BOOL pin = Wh_GetIntSetting(L"topmost") != 0;
    EnumWindows(PinWindowProc, pin);
}
