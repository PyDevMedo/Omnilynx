// ==WindhawkMod==
// @id              win-system-ram-trimmer
// @name            Explorer Working-Set RAM Trimmer
// @description     Periodically trims Explorer working-set memory using event-driven waitable timers
// @version         1.0.0
// @author          Omnilynx
// @include         explorer.exe
// @compilerOptions -luser32 -lpsapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Explorer Working-Set RAM Trimmer
Uses a zero-CPU waitable timer (`CreateWaitableTimerW`) + `EmptyWorkingSet` to gently trim unused Explorer memory pages without stutter or busy-wait loops.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- trimIntervalSec: 30
  $name: Trim Interval (seconds)
*/
// ==/WindhawkModSettings==

#include "omnilynx_mod_iface.hpp"
#include <psapi.h>
#include <atomic>

namespace {

class SystemRamTrimmerMod final : public omnilynx::IMod {
public:
    const wchar_t* Id() const noexcept override { return L"win-system-ram-trimmer"; }
    const wchar_t* Name() const noexcept override { return L"Explorer Working-Set RAM Trimmer"; }
    const wchar_t* Version() const noexcept override { return L"1.0.0"; }

    bool OnInit() override {
        m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!m_stopEvent) return false;
        EmptyWorkingSet(GetCurrentProcess());
        m_thread = CreateThread(nullptr, 0, WorkerProc, this, 0, nullptr);
        return m_thread != nullptr;
    }

    void OnUninit() noexcept override {
        if (m_stopEvent) {
            SetEvent(m_stopEvent);
        }
        if (m_thread) {
            WaitForSingleObject(m_thread, 2000);
            CloseHandle(m_thread);
            m_thread = nullptr;
        }
        if (m_stopEvent) {
            CloseHandle(m_stopEvent);
            m_stopEvent = nullptr;
        }
    }

    void OnSettingsChanged() override {
        int sec = Wh_GetIntSetting(L"trimIntervalSec");
        if (sec < 5 || sec > 300) sec = 30;
        m_intervalMs.store(static_cast<DWORD>(sec * 1000), std::memory_order_relaxed);
    }

private:
    static DWORD WINAPI WorkerProc(LPVOID param) {
        auto* self = static_cast<SystemRamTrimmerMod*>(param);
        for (;;) {
            DWORD waitMs = self->m_intervalMs.load(std::memory_order_relaxed);
            if (WaitForSingleObject(self->m_stopEvent, waitMs) == WAIT_OBJECT_0) {
                break;
            }
            EmptyWorkingSet(GetCurrentProcess());
        }
        return 0;
    }

    HANDLE m_stopEvent = nullptr;
    HANDLE m_thread = nullptr;
    std::atomic<DWORD> m_intervalMs{30000};
};

} // namespace

OMNILYNX_REGISTER_MOD(SystemRamTrimmerMod)
