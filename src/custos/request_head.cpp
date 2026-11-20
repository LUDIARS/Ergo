#include "request_head.h"

#include <lorica/checked_size.hpp>

#include <cctype>
#include <optional>

namespace ergo::custos::detail {

namespace {

bool iequals(const std::string& a, const char* b) {
    std::size_t i = 0;
    for (; i < a.size() && b[i] != '\0'; ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return i == a.size() && b[i] == '\0';
}

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

/// 10 進の数字列 → size_t。数字以外・空・size_t の桁あふれは nullopt。
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

} // namespace

HeadStatus parse_request_head(const std::string& head, const RequestLimits& limits,
                              HttpRequest& out, std::size_t& content_length) {
    content_length = 0;
    const auto header_end = head.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return head.size() >= limits.max_head ? HeadStatus::HeadTooLarge : HeadStatus::Malformed;
    }
    // 終端の 4 バイトを含めて max_head まで。
    const auto head_bytes = lorica::checked_add(header_end, 4);
    if (!head_bytes || *head_bytes > limits.max_head) return HeadStatus::HeadTooLarge;

    const auto first_eol = head.find("\r\n");
    const std::string line = head.substr(0, first_eol);
    const auto sp1 = line.find(' ');
    if (sp1 == std::string::npos || sp1 == 0) return HeadStatus::Malformed;
    const auto sp2 = line.find(' ', sp1 + 1);
    if (sp2 == std::string::npos || sp2 == sp1 + 1) return HeadStatus::Malformed;
    out.method = line.substr(0, sp1);
    out.path   = line.substr(sp1 + 1, sp2 - sp1 - 1);

    std::optional<std::size_t> declared;
    std::size_t pos = first_eol + 2;
    while (pos < header_end) {
        const auto eol = head.find("\r\n", pos);
        if (eol == std::string::npos || eol > header_end) break;
        const std::string field = head.substr(pos, eol - pos);
        pos = eol + 2;
        const auto colon = field.find(':');
        if (colon == std::string::npos || colon == 0) return HeadStatus::Malformed;
        if (!iequals(field.substr(0, colon), "Content-Length")) continue;
        const auto value = parse_decimal(trim(field.substr(colon + 1)));
        if (!value || (declared && *declared != *value)) return HeadStatus::Malformed;
        declared = value;
    }
    content_length = declared.value_or(0);
    if (content_length > limits.max_body) return HeadStatus::BodyTooLarge;
    return HeadStatus::Ok;
}

} // namespace ergo::custos::detail
