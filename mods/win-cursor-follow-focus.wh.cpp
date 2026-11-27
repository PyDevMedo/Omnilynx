// ==WindhawkMod==
// @id              win-cursor-follow-focus
// @name            Cursor Follow Active Window
// @description     Automatically moves the mouse cursor to the center of the newly focused window on Alt+Tab
// @version         1.0.0
// @author          Omnilynx
// @include         explorer.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Cursor Follow Active Window
Uses an event-driven `SetWinEventHook` (`EVENT_SYSTEM_FOREGROUND`) to smoothly center the cursor on the newly focused window with zero idle CPU usage.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- enabled: true
  $name: Center Cursor on Focus Change
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"
#include <atomic>

namespace {

std::atomic<bool> g_cursorFollowEnabled{true};
HWINEVENTHOOK g_focusHook = nullptr;

void CALLBACK OnForegroundChanged(
    HWINEVENTHOOK, DWORD, HWND hwnd, LONG idObject, LONG, DWORD, DWORD)
{
    if (!g_cursorFollowEnabled.load(std::memory_order_relaxed)) return;
    if (idObject != OBJID_WINDOW || !hwnd || !IsWindowVisible(hwnd) || IsIconic(hwnd)) return;

    RECT rc{};
    if (GetWindowRect(hwnd, &rc)) {
        const LONG w = rc.right - rc.left;
        const LONG h = rc.bottom - rc.top;
        if (w > 120 && h > 80) {
            SetCursorPos(rc.left + w / 2, rc.top + h / 2);
        }
    }
}

class CursorFollowFocusMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-cursor-follow-focus"; }
    const wchar_t* Name() const noexcept override { return L"Cursor Follow Active Window"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        g_cursorFollowEnabled.store(true, std::memory_order_relaxed);
        g_focusHook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            nullptr, OnForegroundChanged, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        return g_focusHook != nullptr;
    }

    void OnUninit() noexcept override {
        g_cursorFollowEnabled.store(false, std::memory_order_relaxed);
        if (g_focusHook) {
            UnhookWinEvent(g_focusHook);
            g_focusHook = nullptr;
        }
    }

    void OnSettingsChanged() override {
        g_cursorFollowEnabled.store(Wh_GetIntSetting(L"enabled") != 0, std::memory_order_relaxed);
    }
};

} // namespace

OMNILYNX_REGISTER_MOD(CursorFollowFocusMod)
