#include "ergo/studio/net/http_message.h"

#include <cctype>

namespace ergo::studio::net {

std::optional<std::string> HttpRequest::header(const std::string& lower_name) const {
    const auto it = headers.find(lower_name);
    if (it == headers.end()) return std::nullopt;
    return it->second;
}

HttpResponse HttpResponse::json(int status, std::string body) {
    HttpResponse r;
    r.status = status;
    r.headers["content-type"] = "application/json; charset=utf-8";
    r.body = std::move(body);
    return r;
}

HttpResponse HttpResponse::text(int status, std::string body) {
    HttpResponse r;
    r.status = status;
    r.headers["content-type"] = "text/plain; charset=utf-8";
    r.body = std::move(body);
    return r;
}

HttpResponse HttpResponse::redirect(const std::string& location) {
    HttpResponse r;
    r.status = 302;
    r.headers["location"] = location;
    return r;
}

const char* reason_phrase(int status) {
    switch (status) {
        case 101: return "Switching Protocols";
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 415: return "Unsupported Media Type";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default:  return "Unknown";
    }
}

std::string serialize_response(const HttpResponse& res) {
    std::string out = "HTTP/1.1 " + std::to_string(res.status) + " " + reason_phrase(res.status) + "\r\n";
    for (const auto& [k, v] : res.headers) {
        if (k == "content-length" || k == "connection") continue;
        out += k + ": " + v + "\r\n";
    }
    out += "content-length: " + std::to_string(res.body.size()) + "\r\n";
    out += "connection: close\r\n\r\n";
    out += res.body;
    return out;
}

std::optional<std::string> percent_decode(const std::string& in, bool plus_is_space) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        const char c = in[i];
        if (c == '%') {
            if (i + 2 >= in.size() || !std::isxdigit(static_cast<unsigned char>(in[i + 1]))
                || !std::isxdigit(static_cast<unsigned char>(in[i + 2]))) {
                return std::nullopt;
            }
            out.push_back(static_cast<char>(std::stoi(in.substr(i + 1, 2), nullptr, 16)));
            i += 2;
        } else if (c == '+' && plus_is_space) {
            out.push_back(' ');
        } else {
            out.push_back(c);
        }
    }
    return out;
}

} // namespace ergo::studio::net
