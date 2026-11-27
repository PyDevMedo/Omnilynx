#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <bcrypt.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <utility>

namespace omnilynx {

// RAII wrapper for standard Win32 HANDLEs (supports both nullptr and INVALID_HANDLE_VALUE)
class ScopedHandle {
public:
    ScopedHandle() noexcept = default;
    explicit ScopedHandle(HANDLE h) noexcept : m_handle(h) {}
    ~ScopedHandle() noexcept { reset(); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            reset();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    void reset(HANDLE h = nullptr) noexcept {
        if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
        }
        m_handle = h;
    }

    HANDLE get() const noexcept { return m_handle; }
    bool valid() const noexcept { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }
    explicit operator bool() const noexcept { return valid(); }

private:
    HANDLE m_handle = nullptr;
};

// RAII wrapper for remote process memory allocated via VirtualAllocEx
class ScopedRemoteMem {
public:
    ScopedRemoteMem(HANDLE hProcess, LPVOID addr) noexcept
        : m_process(hProcess), m_addr(addr) {}
    ~ScopedRemoteMem() noexcept { reset(); }

    ScopedRemoteMem(const ScopedRemoteMem&) = delete;
    ScopedRemoteMem& operator=(const ScopedRemoteMem&) = delete;

    void reset() noexcept {
        if (m_process && m_addr) {
            VirtualFreeEx(m_process, m_addr, 0, MEM_RELEASE);
            m_addr = nullptr;
        }
    }

    LPVOID release() noexcept {
        LPVOID tmp = m_addr;
        m_addr = nullptr;
        return tmp;
    }

    LPVOID get() const noexcept { return m_addr; }
    explicit operator bool() const noexcept { return m_addr != nullptr; }

private:
    HANDLE m_process = nullptr;
    LPVOID m_addr = nullptr;
};

// RAII wrapper for HINTERNET handles
class ScopedWinHttp {
public:
    ScopedWinHttp() noexcept = default;
    explicit ScopedWinHttp(HINTERNET h) noexcept : m_h(h) {}
    ~ScopedWinHttp() noexcept { reset(); }

    ScopedWinHttp(const ScopedWinHttp&) = delete;
    ScopedWinHttp& operator=(const ScopedWinHttp&) = delete;

    ScopedWinHttp(ScopedWinHttp&& other) noexcept : m_h(other.m_h) {
        other.m_h = nullptr;
    }
    ScopedWinHttp& operator=(ScopedWinHttp&& other) noexcept {
        if (this != &other) {
            reset();
            m_h = other.m_h;
            other.m_h = nullptr;
        }
        return *this;
    }

    void reset(HINTERNET h = nullptr) noexcept {
        if (m_h) {
            WinHttpCloseHandle(m_h);
        }
        m_h = h;
    }

    HINTERNET get() const noexcept { return m_h; }
    explicit operator bool() const noexcept { return m_h != nullptr; }

private:
    HINTERNET m_h = nullptr;
};

// RAII wrapper for BCrypt algorithm provider
class ScopedBCryptAlg {
public:
    ScopedBCryptAlg() noexcept = default;
    ~ScopedBCryptAlg() noexcept { reset(); }
    ScopedBCryptAlg(const ScopedBCryptAlg&) = delete;
    ScopedBCryptAlg& operator=(const ScopedBCryptAlg&) = delete;

    BCRYPT_ALG_HANDLE* put() noexcept { reset(); return &m_alg; }
    BCRYPT_ALG_HANDLE get() const noexcept { return m_alg; }
    void reset() noexcept {
        if (m_alg) {
            BCryptCloseAlgorithmProvider(m_alg, 0);
            m_alg = nullptr;
        }
    }

private:
    BCRYPT_ALG_HANDLE m_alg = nullptr;
};

// RAII wrapper for BCrypt hash handle
class ScopedBCryptHash {
public:
    ScopedBCryptHash() noexcept = default;
    ~ScopedBCryptHash() noexcept { reset(); }
    ScopedBCryptHash(const ScopedBCryptHash&) = delete;
    ScopedBCryptHash& operator=(const ScopedBCryptHash&) = delete;

    BCRYPT_HASH_HANDLE* put() noexcept { reset(); return &m_hash; }
    BCRYPT_HASH_HANDLE get() const noexcept { return m_hash; }
    void reset() noexcept {
        if (m_hash) {
            BCryptDestroyHash(m_hash);
            m_hash = nullptr;
        }
    }

private:
    BCRYPT_HASH_HANDLE m_hash = nullptr;
};

// RAII wrapper for CoTaskMemFree (used by SHGetKnownFolderPath)
template <typename T>
class ScopedCoTaskMem {
public:
    ScopedCoTaskMem() noexcept = default;
    ~ScopedCoTaskMem() noexcept { reset(); }
    ScopedCoTaskMem(const ScopedCoTaskMem&) = delete;
    ScopedCoTaskMem& operator=(const ScopedCoTaskMem&) = delete;

    T** put() noexcept { reset(); return &m_ptr; }
    T* get() const noexcept { return m_ptr; }
    void reset() noexcept {
        if (m_ptr) {
            CoTaskMemFree(m_ptr);
            m_ptr = nullptr;
        }
    }

private:
    T* m_ptr = nullptr;
};

// RAII wrapper for HMODULE loaded via LoadLibraryW
class ScopedLibrary {
public:
    ScopedLibrary() noexcept = default;
    explicit ScopedLibrary(HMODULE mod) noexcept : m_mod(mod) {}
    ~ScopedLibrary() noexcept { reset(); }
    ScopedLibrary(const ScopedLibrary&) = delete;
    ScopedLibrary& operator=(const ScopedLibrary&) = delete;

    void reset(HMODULE mod = nullptr) noexcept {
        if (m_mod) {
            FreeLibrary(m_mod);
        }
        m_mod = mod;
    }

    HMODULE get() const noexcept { return m_mod; }
    explicit operator bool() const noexcept { return m_mod != nullptr; }

private:
    HMODULE m_mod = nullptr;
};

// Recursively create directory tree (Unicode-safe)
inline bool EnsureDirectoryTreeW(const std::wstring& dirPath) {
    if (dirPath.empty()) return false;
    DWORD attrs = GetFileAttributesW(dirPath.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }

    size_t pos = 0;
    while ((pos = dirPath.find_first_of(L"\\/", pos + 1)) != std::wstring::npos) {
        std::wstring sub = dirPath.substr(0, pos);
        if (sub.size() == 2 && sub[1] == L':') continue; // Drive root e.g. C:
        CreateDirectoryW(sub.c_str(), nullptr);
    }
    if (CreateDirectoryW(dirPath.c_str(), nullptr)) return true;
    attrs = GetFileAttributesW(dirPath.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY));
}

// Resolve %LOCALAPPDATA%\Omnilynx via SHGetKnownFolderPath(FOLDERID_LocalAppData)
inline std::wstring GetLocalAppDataOmnilynxDir() {
    ScopedCoTaskMem<wchar_t> knownPath;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, knownPath.put())) &&
        knownPath.get()) {
        std::wstring dir(knownPath.get());
        if (!dir.empty() && dir.back() != L'\\') dir.push_back(L'\\');
        dir += L"Omnilynx";
        EnsureDirectoryTreeW(dir);
        return dir;
    }

    wchar_t buf[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        std::wstring dir(buf);
        if (dir.back() != L'\\') dir.push_back(L'\\');
        dir += L"Omnilynx";
        EnsureDirectoryTreeW(dir);
        return dir;
    }
    return L".\\Omnilynx";
}

// Check if current process has Administrator (elevated) token
inline bool IsProcessElevated() noexcept {
    ScopedHandle token;
    HANDLE rawToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &rawToken)) {
        return false;
    }
    token.reset(rawToken);
    TOKEN_ELEVATION elev{};
    DWORD retLen = 0;
    if (!GetTokenInformation(token.get(), TokenElevation, &elev, sizeof(elev), &retLen)) {
        return false;
    }
    return elev.TokenIsElevated != 0;
}

// Determine whether a target path requires UAC elevation (Program Files, Windows, System32)
inline bool PathRequiresElevation(const std::wstring& path) {
    wchar_t lower[MAX_PATH * 2] = {};
    size_t len = path.size() < (MAX_PATH * 2 - 1) ? path.size() : (MAX_PATH * 2 - 1);
    for (size_t i = 0; i < len; ++i) {
        wchar_t c = path[i];
        lower[i] = (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c + 32) : c;
    }
    std::wstring p(lower, len);
    return p.find(L"\\windows\\") != std::wstring::npos ||
           p.find(L"\\program files\\") != std::wstring::npos ||
           p.find(L"\\program files (x86)\\") != std::wstring::npos ||
           p.find(L"\\programdata\\") != std::wstring::npos;
}

// Atomic file replacement with optional backup for one-click rollback
inline bool AtomicReplaceFileW(
    const std::wstring& stagedTempFile,
    const std::wstring& targetFile,
    const std::wstring& backupFile = L"")
{
    if (!backupFile.empty() && GetFileAttributesW(targetFile.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (!CopyFileW(targetFile.c_str(), backupFile.c_str(), FALSE)) {
            return false;
        }
    }
    return MoveFileExW(
        stagedTempFile.c_str(),
        targetFile.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

// Rollback target file from backup
inline bool RestoreBackupFileW(const std::wstring& backupFile, const std::wstring& targetFile) {
    if (GetFileAttributesW(backupFile.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return MoveFileExW(
        backupFile.c_str(),
        targetFile.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

// Escape UTF-8 string for safe JSON output
inline std::string EscapeJsonUtf8(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (unsigned char c : in) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

inline std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), out.data(), needed, nullptr, nullptr);
    return out;
}

inline std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), needed);
    return out;
}

// Format Win32 GetLastError() and append to %LOCALAPPDATA%\Omnilynx\error.log
inline std::string FormatWin32ErrorUtf8(DWORD err) {
    if (err == 0) return "0 (No Win32 error)";
    LPWSTR msgBuf = nullptr;
    DWORD len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&msgBuf),
        0,
        nullptr);
    std::string result = std::to_string(err);
    if (len > 0 && msgBuf) {
        std::wstring ws(msgBuf, len);
        while (!ws.empty() && (ws.back() == L'\r' || ws.back() == L'\n' || ws.back() == L' ')) {
            ws.pop_back();
        }
        result += " (" + WideToUtf8(ws) + ")";
        LocalFree(msgBuf);
    }
    return result;
}

inline void AppendErrorLog(const std::string& context, DWORD win32Err, const std::string& detail = "") {
    std::wstring logPath = GetLocalAppDataOmnilynxDir() + L"\\error.log";
    ScopedHandle hFile(CreateFileW(
        logPath.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (!hFile) return;

    SYSTEMTIME st{};
    GetLocalTime(&st);
    char header[128];
    snprintf(header, sizeof(header), "[%04u-%02u-%02u %02u:%02u:%02u] ",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::string line = header + context + " | Win32Error=" + FormatWin32ErrorUtf8(win32Err);
    if (!detail.empty()) {
        line += " | " + detail;
    }
    line += "\r\n";

    DWORD written = 0;
    WriteFile(hFile.get(), line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
}

} // namespace omnilynx
