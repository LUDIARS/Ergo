#include "ergo/http/client.hpp"

#include "curl_global.hpp"

#include <curl/curl.h>

#include <string>

namespace ergo::http {
namespace {

// libcurl write callback — append the received bytes to the target string.
size_t write_body(char* ptr, size_t size, size_t nmemb, void* userdata) {
    const size_t bytes = size * nmemb;
    static_cast<std::string*>(userdata)->append(ptr, bytes);
    return bytes;
}

// libcurl header callback — one call per header line (incl. the status line and
// the terminating blank line). We keep only "Name: Value" lines.
size_t collect_header(char* buffer, size_t size, size_t nitems, void* userdata) {
    const size_t bytes = size * nitems;
    auto*        out   = static_cast<Headers*>(userdata);

    std::string line(buffer, bytes);
    // Strip trailing CRLF.
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }
    const size_t colon = line.find(':');
    if (colon != std::string::npos) {
        std::string name  = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        // Trim one leading space after the colon (the common case).
        const size_t start = value.find_first_not_of(' ');
        value = (start == std::string::npos) ? std::string{} : value.substr(start);
        out->emplace_back(std::move(name), std::move(value));
    }
    return bytes;
}

/// libcurl-backed IHttpClient. One easy handle per send (no pooling — that is
/// explicitly out of scope for this minimal client).
class CurlClient final : public IHttpClient {
public:
    CurlClient() { detail::ensure_curl_global_init(); }

    Response send(const Request& req) override {
        Response resp;

        CURL* curl = curl_easy_init();
        if (curl == nullptr) {
            resp.error = "curl_easy_init failed";
            return resp;
        }

        curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "ergo_http/1.0");
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);  // thread-safe timeouts
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, req.follow_redirects ? 1L : 0L);
        if (req.timeout_ms > 0) {
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, req.timeout_ms);
        }

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, collect_header);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp.headers);

        if (req.method == Method::Post) {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                             static_cast<long>(req.body.size()));
        }

        // Assemble request headers (Content-Type + caller-supplied).
        curl_slist* header_list = nullptr;
        if (!req.content_type.empty()) {
            const std::string ct = "Content-Type: " + req.content_type;
            header_list = curl_slist_append(header_list, ct.c_str());
        }
        for (const auto& h : req.headers) {
            const std::string line = h.first + ": " + h.second;
            header_list = curl_slist_append(header_list, line.c_str());
        }
        if (header_list != nullptr) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        }

        const CURLcode rc = curl_easy_perform(curl);
        if (rc == CURLE_OK) {
            long status = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
            resp.ok     = true;
            resp.status = status;
        } else {
            resp.ok    = false;
            resp.error = curl_easy_strerror(rc);
        }

        if (header_list != nullptr) {
            curl_slist_free_all(header_list);
        }
        curl_easy_cleanup(curl);
        return resp;
    }
};

} // namespace

std::unique_ptr<IHttpClient> make_curl_client() {
    return std::unique_ptr<IHttpClient>(new CurlClient());
}

} // namespace ergo::http
