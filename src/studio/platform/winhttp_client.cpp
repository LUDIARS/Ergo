// WinHTTP implementation of IHttpClient (Windows only).
#include "ergo/studio/platform/http_client.h"

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <string>

namespace ergo::studio::platform {
namespace {

/// Upper bound of a response body, the same as the libcurl backend's
/// (http_request_check.h kMaxResponseBytes): a larger body is an error, not memory.
constexpr std::size_t kMaxResponseBytes = 64u * 1024u * 1024u;

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string last_error(const char* what) { return std::string(what) + " failed (error " + std::to_string(GetLastError()) + ")"; }

/// Closes a WinHTTP handle on scope exit.
struct Handle {
    HINTERNET h = nullptr;
    explicit Handle(HINTERNET x) : h(x) {}
    ~Handle() {
        if (h) WinHttpCloseHandle(h);
    }
    Handle(const Handle&)            = delete;
    Handle& operator=(const Handle&) = delete;
};

class WinHttpClient final : public IHttpClient {
public:
    WinHttpClient()
        : session_(WinHttpOpen(L"ergo-studio/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0)) {}

    HttpClientResponse send(const HttpClientRequest& req) override {
        HttpClientResponse out;
        if (!session_.h) { out.error = last_error("WinHttpOpen"); return out; }

        const std::wstring wurl = widen(req.url);
        URL_COMPONENTS uc{};
        uc.dwStructSize      = sizeof(uc);
        uc.dwHostNameLength  = static_cast<DWORD>(-1);
        uc.dwUrlPathLength   = static_cast<DWORD>(-1);
        uc.dwExtraInfoLength = static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) { out.error = "invalid URL: " + req.url; return out; }
        const bool https = uc.nScheme == INTERNET_SCHEME_HTTPS;
        const std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
        std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
        if (uc.lpszExtraInfo) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
        if (path.empty()) path = L"/";

        WinHttpSetTimeouts(session_.h, req.timeout_ms, req.timeout_ms, req.timeout_ms, req.timeout_ms);
        Handle conn(WinHttpConnect(session_.h, host.c_str(), uc.nPort, 0));
        if (!conn.h) { out.error = last_error("WinHttpConnect"); return out; }
        Handle r(WinHttpOpenRequest(conn.h, widen(req.method).c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                    WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0));
        if (!r.h) { out.error = last_error("WinHttpOpenRequest"); return out; }

        std::wstring headers;
        for (const auto& [k, v] : req.headers) headers += widen(k) + L": " + widen(v) + L"\r\n";
        const BOOL sent = WinHttpSendRequest(r.h, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                                             headers.empty() ? 0 : static_cast<DWORD>(-1),
                                             req.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(req.body.data()),
                                             static_cast<DWORD>(req.body.size()), static_cast<DWORD>(req.body.size()), 0);
        if (!sent || !WinHttpReceiveResponse(r.h, nullptr)) {
            out.timed_out = GetLastError() == ERROR_WINHTTP_TIMEOUT;
            out.error     = last_error("HTTP request");
            return out;
        }

        DWORD status = 0, len = sizeof(status);
        WinHttpQueryHeaders(r.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &len, WINHTTP_NO_HEADER_INDEX);
        out.status = static_cast<int>(status);
        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(r.h, &avail)) { out.error = last_error("read"); return out; }
            if (avail == 0) break;
            const std::size_t at = out.body.size();
            if (avail > kMaxResponseBytes || at > kMaxResponseBytes - avail) {
                out.error = "response body larger than " + std::to_string(kMaxResponseBytes) + " bytes";
                return out;
            }
            out.body.resize(at + avail);
            DWORD got = 0;
            if (!WinHttpReadData(r.h, &out.body[at], avail, &got)) { out.error = last_error("read"); return out; }
            out.body.resize(at + got);
        }
        out.transport_ok = true;
        return out;
    }

private:
    Handle session_;
};

} // namespace

std::shared_ptr<IHttpClient> make_system_http_client() { return std::make_shared<WinHttpClient>(); }

} // namespace ergo::studio::platform
