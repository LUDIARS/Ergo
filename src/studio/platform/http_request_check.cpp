#include "http_request_check.h"

#include <cctype>
#include <string>

namespace ergo::studio::platform::detail {
namespace {

bool starts_with_ci(const std::string& s, const char* prefix) {
    std::size_t i = 0;
    for (; prefix[i] != '\0'; ++i) {
        if (i >= s.size()) return false;
        if (std::tolower(static_cast<unsigned char>(s[i])) != prefix[i]) return false;
    }
    return true;
}

bool equals_ci(const std::string& a, const char* lower) {
    return a.size() == std::char_traits<char>::length(lower) && starts_with_ci(a, lower);
}

/// RFC 9110 tchar.
bool is_token_char(char c) {
    const auto u = static_cast<unsigned char>(c);
    if (std::isalnum(u)) return true;
    switch (c) {
    case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+':
    case '-': case '.': case '^': case '_': case '`': case '|': case '~':
        return true;
    default:
        return false;
    }
}

bool is_token(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!is_token_char(c)) return false;
    }
    return true;
}

bool has_line_break_or_nul(const std::string& s) {
    for (char c : s) {
        if (c == '\r' || c == '\n' || c == '\0') return true;
    }
    return false;
}

/// No spaces or control characters anywhere in the URL (curl would otherwise reject or reinterpret them).
bool url_chars_ok(const std::string& url) {
    for (char c : url) {
        const auto u = static_cast<unsigned char>(c);
        if (u <= 0x20 || u == 0x7F) return false;
    }
    return true;
}

RequestCheck refuse(std::string why) {
    RequestCheck r;
    r.error = std::move(why);
    return r;
}

} // namespace

RequestCheck check_http_request(const HttpClientRequest& req) {
    CheckedRequest out;
    std::size_t    authority = 0;
    if (starts_with_ci(req.url, "https://")) {
        out.https = true;
        authority = 8;
    } else if (starts_with_ci(req.url, "http://")) {
        authority = 7;
    } else {
        return refuse("invalid URL: only http:// and https:// are supported");
    }
    if (!url_chars_ok(req.url)) return refuse("invalid URL: it contains spaces or control characters");
    const std::size_t host_end = req.url.find_first_of("/?#", authority);
    const std::string host     = req.url.substr(authority, host_end == std::string::npos ? std::string::npos : host_end - authority);
    if (host.empty() || host.front() == ':') return refuse("invalid URL: no host");
    if (host.find('@') != std::string::npos) return refuse("invalid URL: user information is not supported");

    if (req.method.empty()) return refuse("invalid method: empty");
    for (char c : req.method) {
        if (c < 'A' || c > 'Z') return refuse("invalid method: only upper-case letters are allowed");
    }
    out.is_get  = req.method == "GET";
    out.is_post = req.method == "POST";

    bool has_content_type = false, has_expect = false;
    for (const auto& [name, value] : req.headers) {
        if (!is_token(name)) return refuse("invalid header name");
        if (has_line_break_or_nul(value)) return refuse("invalid header value for " + name + ": line breaks are not allowed");
        has_content_type = has_content_type || equals_ci(name, "content-type");
        has_expect       = has_expect || equals_ci(name, "expect");
        // libcurl reads "Name:" as "drop this header"; "Name;" sends it with an empty value.
        out.header_lines.push_back(value.empty() ? name + ";" : name + ": " + value);
    }
    // Send what the caller asked for and nothing libcurl would add on its own (as WinHTTP does):
    // no form Content-Type for a POST body, no "Expect: 100-continue" round trip.
    if (!has_content_type) out.header_lines.emplace_back("Content-Type:");
    if (!has_expect) out.header_lines.emplace_back("Expect:");
    if (req.timeout_ms <= 0) return refuse("invalid timeout: it must be > 0 ms");

    RequestCheck r;
    r.request = std::move(out);
    return r;
}

bool append_capped(std::string& body, const char* data, std::size_t len, std::size_t limit) {
    if (len > limit || body.size() > limit - len) return false;
    body.append(data, len);
    return true;
}

} // namespace ergo::studio::platform::detail
