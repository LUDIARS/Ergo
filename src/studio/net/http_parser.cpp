#include "ergo/studio/net/http_parser.h"

#include <lorica/checked_size.hpp>
#include <lorica/input_budget.hpp>

#include <algorithm>
#include <cctype>
#include <optional>

namespace ergo::studio::net {
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

/// Decimal digits -> size_t; nullopt on any other character, empty, or overflow.
std::optional<std::size_t> parse_decimal(const std::string& s) {
    if (s.empty()) return std::nullopt;
    std::size_t value = 0;
    for (const char c : s) {
        if (c < '0' || c > '9') return std::nullopt;
        const auto scaled = lorica::checked_multiply(value, 10);
        const auto next = scaled ? lorica::checked_add(*scaled, static_cast<std::size_t>(c - '0')) : std::nullopt;
        if (!next) return std::nullopt;
        value = *next;
    }
    return value;
}

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

} // namespace

ParseState HttpRequestParser::fail(int status) {
    error_status_ = status;
    return ParseState::Error;
}

int HttpRequestParser::parse_head(const std::string& head) {
    // One budget per request: every header line and query parameter is one
    // element and one map entry; the bytes are already bounded by max_head.
    lorica::input_budget budget({limits_.max_head, limits_.max_fields, limits_.max_fields, 0});
    std::size_t pos = head.find("\r\n");
    const std::string line = head.substr(0, pos);
    const auto sp1 = line.find(' ');
    const auto sp2 = line.rfind(' ');
    if (sp1 == std::string::npos || sp2 == sp1) return 400;
    req_.method = line.substr(0, sp1);
    req_.target = line.substr(sp1 + 1, sp2 - sp1 - 1);
    const std::string version = line.substr(sp2 + 1);
    if (version != "HTTP/1.1" && version != "HTTP/1.0") return 400;
    if (req_.target.empty() || req_.target[0] != '/') return 400;

    const auto q = req_.target.find('?');
    auto path = percent_decode(req_.target.substr(0, q), false);
    if (!path) return 400;
    req_.path = *path;
    if (q != std::string::npos) {
        const std::string qs = req_.target.substr(q + 1);
        std::size_t start = 0;
        while (start <= qs.size()) {
            const auto amp = qs.find('&', start);
            const std::string pair = qs.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
            if (!pair.empty()) {
                if (!budget.consume(pair.size(), 1, 1)) return 431;
                const auto eq = pair.find('=');
                auto k = percent_decode(pair.substr(0, eq), true);
                auto v = percent_decode(eq == std::string::npos ? std::string{} : pair.substr(eq + 1), true);
                if (!k || !v) return 400;
                req_.query[*k] = *v;
            }
            if (amp == std::string::npos) break;
            start = amp + 1;
        }
    }

    while (pos != std::string::npos && pos + 2 < head.size()) {
        const auto next = head.find("\r\n", pos + 2);
        const std::string h = head.substr(pos + 2, next == std::string::npos ? std::string::npos : next - pos - 2);
        pos = next;
        if (h.empty()) continue;
        if (!budget.consume(h.size(), 1, 1)) return 431;
        const auto colon = h.find(':');
        if (colon == std::string::npos || colon == 0) return 400;
        const std::string name = lower(h.substr(0, colon));
        const std::string value = trim(h.substr(colon + 1));
        auto it = req_.headers.find(name);
        if (it == req_.headers.end()) req_.headers.emplace(name, value);
        else it->second += ", " + value;
    }
    return 0;
}

ParseState HttpRequestParser::feed(const char* data, std::size_t len) {
    buf_.append(data, len);
    if (!head_done_) {
        const auto end = buf_.find("\r\n\r\n");
        if (end == std::string::npos) {
            return buf_.size() > limits_.max_head ? fail(431) : ParseState::NeedMore;
        }
        if (end > limits_.max_head) return fail(431);
        if (const int status = parse_head(buf_.substr(0, end + 2)); status != 0) return fail(status);
        head_done_  = true;
        body_start_ = end + 4;

        if (const auto te = req_.header("transfer-encoding"); te && lower(*te) != "identity") return fail(501);
        if (const auto cl = req_.header("content-length")) {
            // Digits only (a repeated header was joined with ", " and fails here).
            const auto declared = parse_decimal(*cl);
            if (!declared) return fail(cl->empty() || !std::all_of(cl->begin(), cl->end(),
                                           [](unsigned char c) { return std::isdigit(c); }) ? 400 : 413);
            if (*declared > limits_.max_body) return fail(413);
            body_len_ = *declared;
        }
    }
    if (buf_.size() - body_start_ < body_len_) return ParseState::NeedMore;
    req_.body = buf_.substr(body_start_, body_len_);
    return ParseState::Complete;
}

std::string HttpRequestParser::take_leftover() {
    const std::size_t used = body_start_ + body_len_;
    std::string rest = used < buf_.size() ? buf_.substr(used) : std::string{};
    buf_.clear();
    return rest;
}

} // namespace ergo::studio::net
