#include "ergo/studio/net/http_parser.h"

#include <algorithm>
#include <cctype>

namespace ergo::studio::net {
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
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

bool HttpRequestParser::parse_head(const std::string& head) {
    std::size_t pos = head.find("\r\n");
    const std::string line = head.substr(0, pos);
    const auto sp1 = line.find(' ');
    const auto sp2 = line.rfind(' ');
    if (sp1 == std::string::npos || sp2 == sp1) return false;
    req_.method = line.substr(0, sp1);
    req_.target = line.substr(sp1 + 1, sp2 - sp1 - 1);
    const std::string version = line.substr(sp2 + 1);
    if (version != "HTTP/1.1" && version != "HTTP/1.0") return false;
    if (req_.target.empty() || req_.target[0] != '/') return false;

    const auto q = req_.target.find('?');
    auto path = percent_decode(req_.target.substr(0, q), false);
    if (!path) return false;
    req_.path = *path;
    if (q != std::string::npos) {
        const std::string qs = req_.target.substr(q + 1);
        std::size_t start = 0;
        while (start <= qs.size()) {
            const auto amp = qs.find('&', start);
            const std::string pair = qs.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
            if (!pair.empty()) {
                const auto eq = pair.find('=');
                auto k = percent_decode(pair.substr(0, eq), true);
                auto v = percent_decode(eq == std::string::npos ? std::string{} : pair.substr(eq + 1), true);
                if (!k || !v) return false;
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
        const auto colon = h.find(':');
        if (colon == std::string::npos || colon == 0) return false;
        const std::string name = lower(h.substr(0, colon));
        const std::string value = trim(h.substr(colon + 1));
        auto it = req_.headers.find(name);
        if (it == req_.headers.end()) req_.headers.emplace(name, value);
        else it->second += ", " + value;
    }
    return true;
}

ParseState HttpRequestParser::feed(const char* data, std::size_t len) {
    buf_.append(data, len);
    if (!head_done_) {
        const auto end = buf_.find("\r\n\r\n");
        if (end == std::string::npos) {
            return buf_.size() > limits_.max_head ? fail(431) : ParseState::NeedMore;
        }
        if (end > limits_.max_head) return fail(431);
        if (!parse_head(buf_.substr(0, end + 2))) return fail(400);
        head_done_  = true;
        body_start_ = end + 4;

        if (const auto te = req_.header("transfer-encoding"); te && lower(*te) != "identity") return fail(501);
        if (const auto cl = req_.header("content-length")) {
            if (cl->empty() || !std::all_of(cl->begin(), cl->end(), [](unsigned char c) { return std::isdigit(c); })
                || cl->size() > 12) {
                return fail(400);
            }
            body_len_ = static_cast<std::size_t>(std::stoull(*cl));
            if (body_len_ > limits_.max_body) return fail(413);
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
