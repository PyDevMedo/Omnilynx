#pragma once

#include "omnilynx_raii.hpp"
#include <dwmapi.h>
#include <shellapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <unordered_set>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

namespace omnilynx {

inline bool IsExternalWindowMod(const wchar_t* modId) noexcept {
    if (!modId) return false;
    static constexpr const wchar_t* kExternalIds[] = {
        L"win-glass-window-opacity",
        L"wendo-window-opacity",
        L"win-mica-acrylic-backdrop",
        L"wendo-translucent-mica",
        L"win-universal-dark-mode",
        L"wendo-dark-titlebars",
        L"win-smooth-window-transitions",
        L"omnilynx-rounded-corners",
        L"omnilynx-always-on-top",
        L"omnilynx-border-accent",
        L"win-switcher-backdrop",
        L"win-active-window-expose",
        L"win-taskbar-floating-style",
        L"win-taskbar-acrylic-tint",
        L"win-taskbar-zorder-control",
        L"win-taskbar-autohide-toggle",
        L"win-alttab-focus-guard",
        L"win-cursor-follow-focus",
        L"win-monitor-snap-focus",
        L"win-taskbar-stability-guard",
        L"win-system-ram-trimmer"
    };
    for (const wchar_t* id : kExternalIds) {
        if (_wcsicmp(modId, id) == 0) return true;
    }
    return false;
}

inline std::vector<HWND> FindTopLevelWindowsForProcess(const wchar_t* exeName) {
    std::unordered_set<DWORD> pids;
    ScopedHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (snap) {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap.get(), &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, exeName) == 0) {
                    pids.insert(pe.th32ProcessID);
                }
            } while (Process32NextW(snap.get(), &pe));
        }
    }

    struct EnumCtx {
        const std::unordered_set<DWORD>* pids;
        std::vector<HWND> hwnds;
    } ctx{ &pids, {} };

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* c = reinterpret_cast<EnumCtx*>(lParam);
        if (!IsWindowVisible(hwnd)) return TRUE;
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (c->pids->count(pid) > 0) {
            c->hwnds.push_back(hwnd);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    return ctx.hwnds;
}

struct ExternalApplyResult {
    bool ok = false;
    int affectedWindows = 0;
    std::string error;
    DWORD win32Error = 0;
};

// Executes window-styling & system mods externally via Win32/DWM APIs (works on UWP/MSIX/protected Windows 11 apps)
inline ExternalApplyResult ApplyExternalMod(
    const wchar_t* modId,
    const wchar_t* targetExe,
    bool enable,
    int paramVal = 0)
{
    ExternalApplyResult res;
    if (!modId) {
        res.error = "Missing modId";
        return res;
    }

    // 1. Taskbar / Shell / System-wide mods (don't require target window enumeration)
    if (_wcsicmp(modId, L"win-taskbar-floating-style") == 0) {
        HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (tray) {
            int backdrop = enable ? (paramVal >= 2 && paramVal <= 4 ? paramVal : 3) : 1;
            int corner = enable ? 2 : 0;
            (void)DwmSetWindowAttribute(tray, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
            (void)DwmSetWindowAttribute(tray, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
            res.affectedWindows = 1;
        }
        res.ok = true;
        return res;
    }

    if (_wcsicmp(modId, L"win-taskbar-acrylic-tint") == 0) {
        const BOOL dark = enable ? TRUE : FALSE;
        const int backdrop = enable ? 3 : 1;
        const wchar_t* classes[] = { L"Shell_TrayWnd", L"NotifyIconOverflowWindow" };
        for (const wchar_t* cls : classes) {
            HWND hwnd = FindWindowW(cls, nullptr);
            if (hwnd) {
                (void)DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
                (void)DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
                res.affectedWindows++;
            }
        }
        res.ok = true;
        return res;
    }

    if (_wcsicmp(modId, L"win-taskbar-zorder-control") == 0) {
        HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (tray) {
            HWND insertAfter = HWND_TOPMOST;
            if (enable) {
                int mode = (paramVal >= 1 && paramVal <= 3) ? paramVal : 2;
                insertAfter = (mode == 2) ? HWND_NOTOPMOST : (mode == 3 ? HWND_BOTTOM : HWND_TOPMOST);
            }
            SetWindowPos(tray, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            res.affectedWindows = 1;
        }
        res.ok = true;
        return res;
    }

    if (_wcsicmp(modId, L"win-taskbar-autohide-toggle") == 0) {
        APPBARDATA abd{};
        abd.cbSize = sizeof(abd);
        abd.lParam = enable ? ABS_AUTOHIDE : 0;
        SHAppBarMessage(ABM_SETSTATE, &abd);
        res.affectedWindows = 1;
        res.ok = true;
        return res;
    }

    if (_wcsicmp(modId, L"win-alttab-focus-guard") == 0) {
        DWORD timeout = enable ? static_cast<DWORD>(paramVal > 0 ? paramVal : 200) : 0;
        SystemParametersInfoW(
            SPI_SETFOREGROUNDLOCKTIMEOUT, 0,
            reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(timeout)), SPIF_SENDCHANGE);
        res.affectedWindows = 1;
        res.ok = true;
        return res;
    }

    if (_wcsicmp(modId, L"win-active-window-expose") == 0) {
        HWND fg = GetForegroundWindow();
        if (fg && IsWindowVisible(fg)) {
            COLORREF color = enable ? RGB(56, 189, 248) : 0xFFFFFFFFU;
            (void)DwmSetWindowAttribute(fg, DWMWA_BORDER_COLOR, &color, sizeof(color));
            res.affectedWindows = 1;
        }
        res.ok = true;
        return res;
    }

    if (_wcsicmp(modId, L"win-cursor-follow-focus") == 0) {
        HWND fg = GetForegroundWindow();
        if (enable && fg && IsWindowVisible(fg) && !IsIconic(fg)) {
            RECT rc{};
            if (GetWindowRect(fg, &rc)) {
                SetCursorPos(rc.left + (rc.right - rc.left) / 2, rc.top + (rc.bottom - rc.top) / 2);
                res.affectedWindows = 1;
            }
        }
        res.ok = true;
        return res;
    }

    if (_wcsicmp(modId, L"win-taskbar-stability-guard") == 0) {
        HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (tray) {
            DWORD_PTR r = 0;
            SendMessageTimeoutW(tray, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 500, &r);
            res.affectedWindows = 1;
        }
        res.ok = true;
        return res;
    }

    if (_wcsicmp(modId, L"win-system-ram-trimmer") == 0) {
        if (targetExe) {
            ScopedHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
            if (snap) {
                PROCESSENTRY32W pe{};
                pe.dwSize = sizeof(pe);
                if (Process32FirstW(snap.get(), &pe)) {
                    do {
                        if (_wcsicmp(pe.szExeFile, targetExe) == 0) {
                            ScopedHandle hProc(OpenProcess(PROCESS_SET_QUOTA | PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID));
                            if (hProc && EmptyWorkingSet(hProc.get())) {
                                res.affectedWindows++;
                            }
                        }
                    } while (Process32NextW(snap.get(), &pe));
                }
            }
        }
        res.ok = true;
        return res;
    }

    // 2. Per-window external DWM / User32 mods (works on UWP/MSIX notepad.exe & standard Win32 apps)
    std::vector<HWND> hwnds = targetExe ? FindTopLevelWindowsForProcess(targetExe) : std::vector<HWND>{};
    if (hwnds.empty() && enable) {
        // Target process is not running yet; caller can launch it first if needed
        res.error = "Target process has no visible windows";
        res.win32Error = 0;
        return res;
    }

    for (HWND hwnd : hwnds) {
        if (_wcsicmp(modId, L"win-glass-window-opacity") == 0 ||
            _wcsicmp(modId, L"wendo-window-opacity") == 0) {
            const BYTE alpha = enable ? static_cast<BYTE>((paramVal >= 50 && paramVal <= 255) ? paramVal : 225) : 255;
            const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            if (alpha < 255) {
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
                SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
            } else {
                SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
            }
            res.affectedWindows++;
        } else if (_wcsicmp(modId, L"win-mica-acrylic-backdrop") == 0 ||
                   _wcsicmp(modId, L"wendo-translucent-mica") == 0 ||
                   _wcsicmp(modId, L"win-switcher-backdrop") == 0) {
            const int type = enable ? ((paramVal >= 2 && paramVal <= 4) ? paramVal : 3) : 1;
            const BOOL dark = enable ? TRUE : FALSE;
            (void)DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
            (void)DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
            res.affectedWindows++;
        } else if (_wcsicmp(modId, L"win-universal-dark-mode") == 0 ||
                   _wcsicmp(modId, L"wendo-dark-titlebars") == 0) {
            const BOOL dark = enable ? TRUE : FALSE;
            const COLORREF border = enable ? RGB(30, 41, 59) : 0xFFFFFFFFU;
            (void)DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
            (void)DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
            res.affectedWindows++;
        } else if (_wcsicmp(modId, L"win-smooth-window-transitions") == 0 ||
                   _wcsicmp(modId, L"omnilynx-rounded-corners") == 0) {
            const int corner = enable ? ((paramVal >= 1 && paramVal <= 3) ? paramVal : 2) : 0;
            const BOOL forceDisabled = enable ? FALSE : TRUE;
            (void)DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &forceDisabled, sizeof(forceDisabled));
            (void)DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
            res.affectedWindows++;
        } else if (_wcsicmp(modId, L"omnilynx-always-on-top") == 0) {
            SetWindowPos(hwnd, enable ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            res.affectedWindows++;
        } else if (_wcsicmp(modId, L"omnilynx-border-accent") == 0) {
            COLORREF color = 0xFFFFFFFFU;
            if (enable) {
                color = (paramVal == 2) ? RGB(16, 185, 129) : (paramVal == 3 ? RGB(99, 102, 241) : RGB(56, 189, 248));
            }
            (void)DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &color, sizeof(color));
            res.affectedWindows++;
        } else if (_wcsicmp(modId, L"win-monitor-snap-focus") == 0) {
            if (enable) {
                POINT pt{};
                if (GetCursorPos(&pt)) {
                    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
                    MONITORINFO mi{};
                    mi.cbSize = sizeof(mi);
                    RECT rc{};
                    if (GetMonitorInfoW(hMon, &mi) && GetWindowRect(hwnd, &rc)) {
                        int w = rc.right - rc.left;
                        int h = rc.bottom - rc.top;
                        int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - w) / 2;
                        int y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - h) / 2;
                        SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                }
            }
            res.affectedWindows++;
        }
    }

    res.ok = true;
    return res;
}

} // namespace omnilynx
