// libcurl implementation of IHttpClient (non-Windows builds with the managed
// libcurl, ERGO_WITH_CURL=ON). TLS comes from the TLS backend libcurl was
// built with (OpenSSL on Linux) and the CA bundle it found at configure time;
// certificate and host-name verification stay on. The proxy follows the
// usual environment variables (https_proxy / http_proxy / no_proxy), as
// WinHTTP follows the system proxy on Windows.
#include "ergo/studio/platform/http_client.h"

#include "curl_http_client.h"
#include "http_request_check.h"

#include <curl/curl.h>

#include <string>

namespace ergo::studio::platform {
namespace {

/// curl_global_init once per process (not thread-safe itself), cleanup at exit.
struct CurlGlobal {
    CURLcode code;
    CurlGlobal() : code(curl_global_init(CURL_GLOBAL_DEFAULT)) {}
    ~CurlGlobal() {
        if (code == CURLE_OK) curl_global_cleanup();
    }
    CurlGlobal(const CurlGlobal&)            = delete;
    CurlGlobal& operator=(const CurlGlobal&) = delete;
};

CURLcode ensure_global_init() {
    static CurlGlobal g;
    return g.code;
}

struct Received {
    std::string body;
    std::size_t limit     = 0;
    bool        too_large = false;
};

size_t on_body(char* data, size_t size, size_t nmemb, void* user) {
    auto*        r   = static_cast<Received*>(user);
    const size_t len = size * nmemb;
    if (!detail::append_capped(r->body, data, len, r->limit)) {
        r->too_large = true;
        return 0;   // aborts the transfer with CURLE_WRITE_ERROR
    }
    return len;
}

/// Frees the easy handle and the header list on scope exit.
struct Transfer {
    CURL*       curl    = curl_easy_init();
    curl_slist* headers = nullptr;
    Transfer() = default;
    ~Transfer() {
        if (headers) curl_slist_free_all(headers);
        if (curl) curl_easy_cleanup(curl);
    }
    Transfer(const Transfer&)            = delete;
    Transfer& operator=(const Transfer&) = delete;
};

bool is_tls_failure(CURLcode rc) {
    switch (rc) {
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_PEER_FAILED_VERIFICATION:
    case CURLE_SSL_CERTPROBLEM:
    case CURLE_SSL_CIPHER:
    case CURLE_SSL_CACERT_BADFILE:
    case CURLE_SSL_ISSUER_ERROR:
    case CURLE_SSL_CRL_BADFILE:
    case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
    case CURLE_SSL_INVALIDCERTSTATUS:
    case CURLE_SSL_ENGINE_INITFAILED:
    case CURLE_SSL_ENGINE_NOTFOUND:
    case CURLE_SSL_ENGINE_SETFAILED:
    case CURLE_SSL_SHUTDOWN_FAILED:
    case CURLE_SSL_CLIENTCERT:
        return true;
    default:
        return false;
    }
}

class CurlHttpClient final : public IHttpClient {
public:
    explicit CurlHttpClient(std::size_t max_response_bytes) : max_response_bytes_(max_response_bytes) {}

    HttpClientResponse send(const HttpClientRequest& req) override {
        HttpClientResponse out;
        const detail::RequestCheck checked = detail::check_http_request(req);
        if (!checked.request) {
            out.error = checked.error;
            return out;
        }
        if (const CURLcode g = ensure_global_init(); g != CURLE_OK) {
            out.error = std::string("curl_global_init failed: ") + curl_easy_strerror(g);
            return out;
        }
        Transfer t;
        if (!t.curl) {
            out.error = "curl_easy_init failed";
            return out;
        }
        for (const auto& line : checked.request->header_lines) {
            curl_slist* next = curl_slist_append(t.headers, line.c_str());
            if (!next) {
                out.error = "out of memory while building the request headers";
                return out;
            }
            t.headers = next;
        }

        Received got;
        got.limit = max_response_bytes_;
        CURL*    c = t.curl;
        // Every setopt below takes values this function controls; a failure would be a libcurl build without
        // HTTP(S), which the configure step rules out, so it is reported as such instead of being ignored.
        CURLcode rc = CURLE_OK;
        auto     set = [&](CURLcode r) {
            if (rc == CURLE_OK) rc = r;
        };
        set(curl_easy_setopt(c, CURLOPT_URL, req.url.c_str()));
        set(curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "http,https"));
        // Like WinHTTP's default redirect policy: follow, but never from https down to http.
        set(curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L));
        set(curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10L));
        set(curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS_STR, checked.request->https ? "https" : "http,https"));
        set(curl_easy_setopt(c, CURLOPT_USERAGENT, "ergo-studio/1"));
        set(curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L));
        set(curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, static_cast<long>(req.timeout_ms)));
        set(curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L));
        set(curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L));
        set(curl_easy_setopt(c, CURLOPT_HTTPHEADER, t.headers));
        set(curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, &on_body));
        set(curl_easy_setopt(c, CURLOPT_WRITEDATA, &got));
        if (checked.request->is_get) {
            set(curl_easy_setopt(c, CURLOPT_HTTPGET, 1L));
        } else {
            if (!checked.request->is_post) set(curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, req.method.c_str()));
            if (checked.request->is_post || !req.body.empty()) {
                set(curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(req.body.size())));
                set(curl_easy_setopt(c, CURLOPT_POSTFIELDS, req.body.data()));
            }
        }
        if (rc != CURLE_OK) {
            out.error = std::string("libcurl does not accept the request options: ") + curl_easy_strerror(rc);
            return out;
        }

        rc = curl_easy_perform(c);
        if (rc != CURLE_OK) {
            out.timed_out = rc == CURLE_OPERATION_TIMEDOUT;
            if (got.too_large) {
                out.error = "response body larger than " + std::to_string(max_response_bytes_) + " bytes";
            } else if (is_tls_failure(rc)) {
                out.error = std::string("TLS failed: ") + curl_easy_strerror(rc);
            } else {
                out.error = curl_easy_strerror(rc);   // generic text: never the URL, the host or a header
            }
            return out;
        }
        long status = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
        out.status       = static_cast<int>(status);
        out.body         = std::move(got.body);
        out.transport_ok = true;
        return out;
    }

private:
    std::size_t max_response_bytes_;
};

} // namespace

namespace detail {
std::shared_ptr<IHttpClient> make_curl_http_client(std::size_t max_response_bytes) {
    return std::make_shared<CurlHttpClient>(max_response_bytes);
}
} // namespace detail

std::shared_ptr<IHttpClient> make_system_http_client() { return detail::make_curl_http_client(detail::kMaxResponseBytes); }

} // namespace ergo::studio::platform
