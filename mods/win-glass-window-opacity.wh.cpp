// ==WindhawkMod==
// @id              win-glass-window-opacity
// @name            Glass Window Opacity
// @description     Makes target application windows semi-transparent with configurable opacity
// @version         1.0.0
// @author          Omnilynx
// @include         notepad.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Glass Window Opacity
Sets a clean semi-transparent glass opacity on all visible windows in the target process and restores full opacity on unload.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- opacity: 225
  $name: Window Opacity (50-255)
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"

namespace {

BOOL CALLBACK ApplyOpacityProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
        const BYTE alpha = static_cast<BYTE>(lParam & 0xFF);
        const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
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

class GlassWindowOpacityMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-glass-window-opacity"; }
    const wchar_t* Name() const noexcept override { return L"Glass Window Opacity"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        int alpha = Wh_GetIntSetting(L"opacity");
        if (alpha < 50 || alpha > 255) alpha = 225;
        EnumWindows(ApplyOpacityProc, static_cast<LPARAM>(alpha));
        return true;
    }

    void OnUninit() noexcept override {
        EnumWindows(ApplyOpacityProc, 255);
    }

    void OnSettingsChanged() override {
        int alpha = Wh_GetIntSetting(L"opacity");
        if (alpha < 50 || alpha > 255) alpha = 225;
        EnumWindows(ApplyOpacityProc, static_cast<LPARAM>(alpha));
    }
};

} // namespace

OMNILYNX_REGISTER_MOD(GlassWindowOpacityMod)
