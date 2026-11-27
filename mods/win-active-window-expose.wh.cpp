// ==WindhawkMod==
// @id              win-active-window-expose
// @name            Active Window Focus Highlight
// @description     Highlights the currently focused window with a crisp DWM accent border
// @version         1.0.0
// @author          Omnilynx
// @include         explorer.exe
// @compilerOptions -luser32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Active Window Focus Highlight
Highlights whichever window is currently active with a crisp cyan/indigo DWM border (`DWMWA_BORDER_COLOR`), restoring the previous window's default border when focus moves.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- highlightColor: 1
  $name: Highlight Color (1=Cyan, 2=Indigo, 3=Emerald)
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"
#include <dwmapi.h>
#include <atomic>

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

namespace {

std::atomic<COLORREF> g_accentColor{RGB(56, 189, 248)};
HWND g_lastFocused = nullptr;
HWINEVENTHOOK g_exposeHook = nullptr;

COLORREF ResolveColor(int preset) noexcept {
    if (preset == 2) return RGB(99, 102, 241);
    if (preset == 3) return RGB(16, 185, 129);
    return RGB(56, 189, 248);
}

void CALLBACK OnExposeFocus(
    HWINEVENTHOOK, DWORD, HWND hwnd, LONG idObject, LONG, DWORD, DWORD)
{
    if (idObject != OBJID_WINDOW || !hwnd || !IsWindowVisible(hwnd)) return;
    if (g_lastFocused && g_lastFocused != hwnd && IsWindow(g_lastFocused)) {
        COLORREF defColor = 0xFFFFFFFFU;
        (void)DwmSetWindowAttribute(g_lastFocused, DWMWA_BORDER_COLOR, &defColor, sizeof(defColor));
    }
    COLORREF active = g_accentColor.load(std::memory_order_relaxed);
    (void)DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &active, sizeof(active));
    g_lastFocused = hwnd;
}

class ActiveWindowExposeMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-active-window-expose"; }
    const wchar_t* Name() const noexcept override { return L"Active Window Focus Highlight"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        g_accentColor.store(ResolveColor(Wh_GetIntSetting(L"highlightColor")), std::memory_order_relaxed);
        g_exposeHook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            nullptr, OnExposeFocus, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        return g_exposeHook != nullptr;
    }

    void OnUninit() noexcept override {
        if (g_exposeHook) {
            UnhookWinEvent(g_exposeHook);
            g_exposeHook = nullptr;
        }
        if (g_lastFocused && IsWindow(g_lastFocused)) {
            COLORREF defColor = 0xFFFFFFFFU;
            (void)DwmSetWindowAttribute(g_lastFocused, DWMWA_BORDER_COLOR, &defColor, sizeof(defColor));
            g_lastFocused = nullptr;
        }
    }

    void OnSettingsChanged() override {
        g_accentColor.store(ResolveColor(Wh_GetIntSetting(L"highlightColor")), std::memory_order_relaxed);
    }
};

} // namespace

OMNILYNX_REGISTER_MOD(ActiveWindowExposeMod)
