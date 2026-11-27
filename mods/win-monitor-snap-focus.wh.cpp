// ==WindhawkMod==
// @id              win-monitor-snap-focus
// @name            Per-Monitor Explorer Window Snap
// @description     Keeps newly opened Explorer windows on the monitor where your mouse cursor currently is
// @version         1.0.0
// @author          Omnilynx
// @include         explorer.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Per-Monitor Explorer Window Snap
When a new Explorer window opens, automatically centers it on the monitor containing your mouse cursor.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- snapToCursorMonitor: true
  $name: Open Explorer on Cursor Monitor
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"
#include <atomic>

namespace {

std::atomic<bool> g_snapEnabled{true};
HWINEVENTHOOK g_showHook = nullptr;

void CALLBACK OnWindowShown(
    HWINEVENTHOOK, DWORD, HWND hwnd, LONG idObject, LONG, DWORD, DWORD)
{
    if (!g_snapEnabled.load(std::memory_order_relaxed)) return;
    if (idObject != OBJID_WINDOW || !hwnd || !IsWindowVisible(hwnd)) return;

    wchar_t cls[64] = {};
    if (!GetClassNameW(hwnd, cls, 64)) return;
    if (_wcsicmp(cls, L"CabinetWClass") != 0) return;

    POINT pt{};
    if (!GetCursorPos(&pt)) return;

    HMONITOR hTargetMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    HMONITOR hWinMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!hTargetMon || hTargetMon == hWinMon) return;

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    RECT winRc{};
    if (GetMonitorInfoW(hTargetMon, &mi) && GetWindowRect(hwnd, &winRc)) {
        const int w = winRc.right - winRc.left;
        const int h = winRc.bottom - winRc.top;
        const int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - w) / 2;
        const int y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - h) / 2;
        SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

class MonitorSnapFocusMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-monitor-snap-focus"; }
    const wchar_t* Name() const noexcept override { return L"Per-Monitor Explorer Window Snap"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        g_snapEnabled.store(true, std::memory_order_relaxed);
        g_showHook = SetWinEventHook(
            EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW,
            nullptr, OnWindowShown, GetCurrentProcessId(), 0, WINEVENT_OUTOFCONTEXT);
        return g_showHook != nullptr;
    }

    void OnUninit() noexcept override {
        g_snapEnabled.store(false, std::memory_order_relaxed);
        if (g_showHook) {
            UnhookWinEvent(g_showHook);
            g_showHook = nullptr;
        }
    }

    void OnSettingsChanged() override {
        g_snapEnabled.store(Wh_GetIntSetting(L"snapToCursorMonitor") != 0, std::memory_order_relaxed);
    }
};

} // namespace

OMNILYNX_REGISTER_MOD(MonitorSnapFocusMod)
