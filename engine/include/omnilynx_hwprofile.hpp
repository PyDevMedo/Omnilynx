#pragma once

#include "omnilynx_raii.hpp"
#include <dxgi.h>
#include <cstdint>
#include <string>

namespace omnilynx {

enum class HardwareTier {
    LowEnd = 0,
    MidRange = 1,
    HighEnd = 2
};

struct HardwareProfile {
    DWORD logicalCores = 1;
    uint64_t totalRamMB = 0;
    uint64_t availRamMB = 0;
    uint64_t gpuVramMB = 0;
    std::wstring gpuName = L"Unknown";
    DWORD osMajor = 10;
    DWORD osMinor = 0;
    DWORD osBuild = 0;
    bool isWindows11 = false;
    HardwareTier tier = HardwareTier::MidRange;

    // Adaptive scaling parameters
    DWORD recommendedThreads = 2;
    size_t ioBufferBytes = 64 * 1024;
    DWORD backgroundPollMs = 15000;
};

namespace detail {
using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOEXW*);
using CreateDXGIFactory1Fn = HRESULT(WINAPI*)(REFIID, void**);
} // namespace detail

inline HardwareProfile DetectHardwareProfile() {
    HardwareProfile prof;

    // 1. CPU cores via GetNativeSystemInfo
    SYSTEM_INFO sysInfo{};
    GetNativeSystemInfo(&sysInfo);
    prof.logicalCores = sysInfo.dwNumberOfProcessors > 0 ? sysInfo.dwNumberOfProcessors : 1;

    // 2. RAM via GlobalMemoryStatusEx
    MEMORYSTATUSEX memStatus{};
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        prof.totalRamMB = memStatus.ullTotalPhys / (1024ULL * 1024ULL);
        prof.availRamMB = memStatus.ullAvailPhys / (1024ULL * 1024ULL);
    }

    // 3. Accurate Windows 10/11 build via RtlGetVersion (immune to compatibility shims)
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        auto rtlGetVersion = reinterpret_cast<detail::RtlGetVersionFn>(
            reinterpret_cast<void*>(GetProcAddress(hNtdll, "RtlGetVersion")));
        if (rtlGetVersion) {
            OSVERSIONINFOEXW osvi{};
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            if (rtlGetVersion(&osvi) == 0) {
                prof.osMajor = osvi.dwMajorVersion;
                prof.osMinor = osvi.dwMinorVersion;
                prof.osBuild = osvi.dwBuildNumber;
                prof.isWindows11 = (osvi.dwMajorVersion >= 10 && osvi.dwBuildNumber >= 22000);
            }
        }
    }

    // 4. GPU VRAM via DXGI (dynamically loaded so dxgi.dll is optional)
    ScopedLibrary dxgiLib(LoadLibraryW(L"dxgi.dll"));
    if (dxgiLib) {
        auto createFactory1 = reinterpret_cast<detail::CreateDXGIFactory1Fn>(
            reinterpret_cast<void*>(GetProcAddress(dxgiLib.get(), "CreateDXGIFactory1")));
        if (createFactory1) {
            IDXGIFactory1* factory = nullptr;
            if (SUCCEEDED(createFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) && factory) {
                IDXGIAdapter1* adapter = nullptr;
                uint64_t bestVram = 0;
                for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
                    DXGI_ADAPTER_DESC1 desc{};
                    if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
                            uint64_t vramMB = static_cast<uint64_t>(desc.DedicatedVideoMemory) / (1024ULL * 1024ULL);
                            if (vramMB >= bestVram) {
                                bestVram = vramMB;
                                prof.gpuVramMB = vramMB;
                                prof.gpuName = desc.Description;
                            }
                        }
                    }
                    adapter->Release();
                    adapter = nullptr;
                }
                factory->Release();
            }
        }
    }

    // 5. Classify tier and compute adaptive scaling guardrails
    if (prof.logicalCores <= 4 || prof.totalRamMB <= 6144) {
        prof.tier = HardwareTier::LowEnd;
        prof.recommendedThreads = 1;
        prof.ioBufferBytes = 32 * 1024;
        prof.backgroundPollMs = 20000;
    } else if (prof.logicalCores >= 12 && prof.totalRamMB >= 16384 && prof.gpuVramMB >= 4096) {
        prof.tier = HardwareTier::HighEnd;
        prof.recommendedThreads = (prof.logicalCores > 8) ? 4 : 2;
        prof.ioBufferBytes = 256 * 1024;
        prof.backgroundPollMs = 10000;
    } else {
        prof.tier = HardwareTier::MidRange;
        prof.recommendedThreads = 2;
        prof.ioBufferBytes = 64 * 1024;
        prof.backgroundPollMs = 15000;
    }

    return prof;
}

inline const char* HardwareTierName(HardwareTier tier) noexcept {
    switch (tier) {
        case HardwareTier::LowEnd:   return "low-end";
        case HardwareTier::MidRange: return "mid-range";
        case HardwareTier::HighEnd:  return "high-end";
    }
    return "mid-range";
}

} // namespace omnilynx
