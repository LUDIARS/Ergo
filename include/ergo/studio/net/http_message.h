#pragma once
/// HTTP/1.1 request and response values exchanged between the Studio server
/// and the plugins. Header names are stored lower-cased.
#include <map>
#include <optional>
#include <string>

namespace ergo::studio::net {

struct HttpRequest {
    std::string                        method;   // "GET", "POST", ...
    std::string                        target;   // raw request-target ("/a/b?x=1")
    std::string                        path;     // percent-decoded path ("/a/b")
    std::map<std::string, std::string> query;    // percent-decoded query pairs
    std::map<std::string, std::string> headers;  // lower-cased names
    std::string                        body;

    /// Header value, or nullopt when absent.
    std::optional<std::string> header(const std::string& lower_name) const;
};

struct HttpResponse {
    int                                status = 200;
    std::map<std::string, std::string> headers;  // e.g. {"content-type", "..."}
    std::string                        body;

    static HttpResponse json(int status, std::string body);
    static HttpResponse text(int status, std::string body);
    static HttpResponse redirect(const std::string& location);
};

/// Reason phrase for the status line ("OK", "Not Found", ...).
const char* reason_phrase(int status);

/// Full wire form of a response (status line, headers, Content-Length, body).
std::string serialize_response(const HttpResponse& res);

/// Percent-decode a URL component (`+` is kept as-is in paths, decoded to
/// space in query values). Returns nullopt on a malformed escape.
std::optional<std::string> percent_decode(const std::string& in, bool plus_is_space);

} // namespace ergo::studio::net
