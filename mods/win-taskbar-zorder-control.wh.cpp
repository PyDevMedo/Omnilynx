// ==WindhawkMod==
// @id              win-taskbar-zorder-control
// @name            Taskbar Z-Order Control
// @description     Controls whether the Windows Taskbar stays Always on Top, Normal, or at the Bottom
// @version         1.0.0
// @author          Omnilynx
// @include         explorer.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Taskbar Z-Order Control
Controls `Shell_TrayWnd` Z-order (`1=Topmost`, `2=Normal`, `3=Bottom`) and restores `HWND_TOPMOST` when unloaded.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- zOrderMode: 2
  $name: Z-Order Mode (1=Topmost, 2=Normal, 3=Bottom)
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"

namespace {

void ApplyTaskbarZOrder(int mode) noexcept {
    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!tray) return;
    HWND insertAfter = HWND_TOPMOST;
    if (mode == 2) insertAfter = HWND_NOTOPMOST;
    else if (mode == 3) insertAfter = HWND_BOTTOM;
    SetWindowPos(tray, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

class TaskbarZOrderMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-taskbar-zorder-control"; }
    const wchar_t* Name() const noexcept override { return L"Taskbar Z-Order Control"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        int mode = Wh_GetIntSetting(L"zOrderMode");
        if (mode < 1 || mode > 3) mode = 2;
        ApplyTaskbarZOrder(mode);
        return true;
    }

    void OnUninit() noexcept override {
        ApplyTaskbarZOrder(1);
    }

    void OnSettingsChanged() override {
        int mode = Wh_GetIntSetting(L"zOrderMode");
        if (mode < 1 || mode > 3) mode = 2;
        ApplyTaskbarZOrder(mode);
    }
};

} // namespace

OMNILYNX_REGISTER_MOD(TaskbarZOrderMod)
