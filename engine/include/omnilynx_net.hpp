#pragma once

#include "omnilynx_raii.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace omnilynx {

// Exclusive GitHub linkage: ONLY raw.githubusercontent.com/PyDevMedo/Omnilynx or direct Release assets (never /blob/ HTML)
inline bool IsAllowedOmnilynxGitHubUrl(const std::wstring& url) {
    static constexpr const wchar_t* kPrefixRaw = L"https://raw.githubusercontent.com/PyDevMedo/Omnilynx/";
    static constexpr const wchar_t* kPrefixRel = L"https://github.com/PyDevMedo/Omnilynx/releases/download/";
    return url.rfind(kPrefixRaw, 0) == 0 || url.rfind(kPrefixRel, 0) == 0;
}

// Compute SHA-256 hex digest using Windows BCrypt (no OpenSSL dependency)
inline bool ComputeSha256Hex(const uint8_t* data, size_t size, std::string& outHex) {
    ScopedBCryptAlg alg;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(alg.put(), BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
        return false;
    }

    DWORD objLen = 0;
    DWORD cbData = 0;
    if (!BCRYPT_SUCCESS(BCryptGetProperty(
            alg.get(), BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen), sizeof(objLen), &cbData, 0))) {
        return false;
    }

    std::vector<uint8_t> hashObj(objLen);
    ScopedBCryptHash hash;
    if (!BCRYPT_SUCCESS(BCryptCreateHash(alg.get(), hash.put(), hashObj.data(), objLen, nullptr, 0, 0))) {
        return false;
    }

    if (size > 0) {
        if (!BCRYPT_SUCCESS(BCryptHashData(hash.get(), const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0))) {
            return false;
        }
    }

    uint8_t digest[32] = {};
    if (!BCRYPT_SUCCESS(BCryptFinishHash(hash.get(), digest, sizeof(digest), 0))) {
        return false;
    }

    static constexpr char kHex[] = "0123456789abcdef";
    outHex.resize(64);
    for (size_t i = 0; i < 32; ++i) {
        outHex[i * 2]     = kHex[(digest[i] >> 4) & 0xF];
        outHex[i * 2 + 1] = kHex[digest[i] & 0xF];
    }

    // Release temporary heap buffer immediately
    hashObj.clear();
    hashObj.shrink_to_fit();
    return true;
}

struct DownloadResult {
    bool ok = false;
    DWORD statusCode = 0;
    std::string sha256;
    std::vector<uint8_t> bytes;
    std::string error;
};

// Native WinHTTP GET restricted strictly to PyDevMedo/Omnilynx GitHub endpoints
inline DownloadResult GitHubFetchBytes(
    const std::wstring& url,
    const std::wstring& expectedSha256Hex = L"",
    DWORD timeoutMs = 10000)
{
    DownloadResult res;
    if (!IsAllowedOmnilynxGitHubUrl(url)) {
        res.error = "Blocked: only PyDevMedo/Omnilynx GitHub URLs are permitted";
        return res;
    }

    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t hostBuf[256] = {};
    wchar_t pathBuf[2048] = {};
    uc.lpszHostName = hostBuf;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = pathBuf;
    uc.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &uc)) {
        res.error = "WinHttpCrackUrl failed";
        return res;
    }

    ScopedWinHttp session(WinHttpOpen(
        L"Omnilynx-Native/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0));
    if (!session) {
        res.error = "WinHttpOpen failed";
        return res;
    }

    WinHttpSetTimeouts(session.get(), timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    ScopedWinHttp connect(WinHttpConnect(session.get(), hostBuf, uc.nPort, 0));
    if (!connect) {
        res.error = "WinHttpConnect failed";
        return res;
    }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    ScopedWinHttp request(WinHttpOpenRequest(
        connect.get(), L"GET", pathBuf, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request) {
        res.error = "WinHttpOpenRequest failed";
        return res;
    }

    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
        res.error = "WinHTTP request/response failed";
        return res;
    }

    DWORD statusSize = sizeof(res.statusCode);
    WinHttpQueryHeaders(
        request.get(),
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &res.statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX);

    if (res.statusCode < 200 || res.statusCode >= 300) {
        res.error = "HTTP status error";
        return res;
    }

    std::vector<uint8_t> chunk;
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(request.get(), &avail) && avail > 0) {
        chunk.resize(avail);
        DWORD readBytes = 0;
        if (!WinHttpReadData(request.get(), chunk.data(), avail, &readBytes)) {
            res.error = "WinHttpReadData failed";
            res.bytes.clear();
            res.bytes.shrink_to_fit();
            return res;
        }
        res.bytes.insert(res.bytes.end(), chunk.begin(), chunk.begin() + readBytes);
    }
    chunk.clear();
    chunk.shrink_to_fit();

    if (!ComputeSha256Hex(res.bytes.data(), res.bytes.size(), res.sha256)) {
        res.error = "SHA-256 calculation failed";
        return res;
    }

    if (!expectedSha256Hex.empty()) {
        std::string exp = WideToUtf8(expectedSha256Hex);
        if (_stricmp(exp.c_str(), res.sha256.c_str()) != 0) {
            res.error = "SHA-256 verification mismatch";
            res.bytes.clear();
            res.bytes.shrink_to_fit();
            return res;
        }
    }

    res.ok = true;
    return res;
}

// Fetch from PyDevMedo/Omnilynx with local disk cache fallback in %LOCALAPPDATA%\Omnilynx\Cache
inline bool FetchModSourceWithCache(
    const std::wstring& modFileName,
    std::vector<uint8_t>& outBytes,
    std::string& outSha256,
    std::string& outError)
{
    std::wstring cacheDir = GetLocalAppDataOmnilynxDir() + L"\\Cache";
    EnsureDirectoryTreeW(cacheDir);
    std::wstring cacheFile = cacheDir + L"\\" + modFileName;
    std::wstring tempFile  = cacheFile + L".tmp";
    std::wstring url = L"https://raw.githubusercontent.com/PyDevMedo/Omnilynx/main/mods/" + modFileName;

    DownloadResult dl = GitHubFetchBytes(url);
    if (dl.ok && !dl.bytes.empty()) {
        ScopedHandle hFile(CreateFileW(tempFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (hFile) {
            DWORD written = 0;
            WriteFile(hFile.get(), dl.bytes.data(), static_cast<DWORD>(dl.bytes.size()), &written, nullptr);
            hFile.reset();
            AtomicReplaceFileW(tempFile, cacheFile);
        }
        outBytes = std::move(dl.bytes);
        outSha256 = std::move(dl.sha256);
        return true;
    }

    // Offline resilience: fall back to cached copy if present
    ScopedHandle hCache(CreateFileW(cacheFile.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (hCache) {
        LARGE_INTEGER sz{};
        if (GetFileSizeEx(hCache.get(), &sz) && sz.QuadPart > 0 && sz.QuadPart < 32 * 1024 * 1024) {
            outBytes.resize(static_cast<size_t>(sz.QuadPart));
            DWORD readBytes = 0;
            if (ReadFile(hCache.get(), outBytes.data(), static_cast<DWORD>(sz.QuadPart), &readBytes, nullptr)) {
                outBytes.resize(readBytes);
                ComputeSha256Hex(outBytes.data(), outBytes.size(), outSha256);
                return true;
            }
        }
    }

    outError = dl.error.empty() ? "Network and cache unavailable" : dl.error;
    return false;
}

} // namespace omnilynx
