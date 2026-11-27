// ==WindhawkMod==
// @id              win-taskbar-stability-guard
// @name            Taskbar Stability & Hung Window Guard
// @description     Prevents hung ghost windows from disrupting Explorer taskbar ordering and responsiveness
// @version         1.0.0
// @author          Omnilynx
// @include         explorer.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Taskbar Stability & Hung Window Guard
Uses an event-driven `SetWinEventHook` (`EVENT_OBJECT_REORDER`) to detect hung top-level windows (`IsHungAppWindow`) and keep the Taskbar responsive.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- guardEnabled: true
  $name: Enable Taskbar Hung Window Guard
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"
#include <atomic>

namespace {

std::atomic<bool> g_guardEnabled{true};
HWINEVENTHOOK g_reorderHook = nullptr;

void CALLBACK OnObjectReorder(
    HWINEVENTHOOK, DWORD, HWND hwnd, LONG idObject, LONG, DWORD, DWORD)
{
    if (!g_guardEnabled.load(std::memory_order_relaxed)) return;
    if (idObject != OBJID_WINDOW || !hwnd) return;
    if (IsHungAppWindow(hwnd)) {
        HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (tray) {
            PostMessageW(tray, WM_NULL, 0, 0);
        }
    }
}

class TaskbarStabilityGuardMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-taskbar-stability-guard"; }
    const wchar_t* Name() const noexcept override { return L"Taskbar Stability & Hung Window Guard"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        g_guardEnabled.store(true, std::memory_order_relaxed);
        g_reorderHook = SetWinEventHook(
            EVENT_OBJECT_REORDER, EVENT_OBJECT_REORDER,
            nullptr, OnObjectReorder, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        return g_reorderHook != nullptr;
    }

    void OnUninit() noexcept override {
        g_guardEnabled.store(false, std::memory_order_relaxed);
        if (g_reorderHook) {
            UnhookWinEvent(g_reorderHook);
            g_reorderHook = nullptr;
        }
    }

    void OnSettingsChanged() override {
        g_guardEnabled.store(Wh_GetIntSetting(L"guardEnabled") != 0, std::memory_order_relaxed);
    }
};

} // namespace

OMNILYNX_REGISTER_MOD(TaskbarStabilityGuardMod)
