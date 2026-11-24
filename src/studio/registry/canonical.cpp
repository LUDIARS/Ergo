#include "canonical.h"

#include "ergo/studio/registry/trust.h"
#include <lorica/input_budget.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace ergo::studio::registry {
namespace {

std::uint32_t next_codepoint(std::string_view text, std::size_t& offset, bool allow_surrogates) {
    const auto lead = static_cast<unsigned char>(text[offset++]);
    if (lead < 0x80) return lead;
    const unsigned count = lead < 0xe0 ? 1 : lead < 0xf0 ? 2 : 3;
    if (lead < 0xc2 || lead > 0xf4 || text.size() - offset < count) throw portus::JsonError("invalid UTF-8");
    std::uint32_t value = lead & (0x7fU >> (count + 1));
    for (unsigned i = 0; i < count; ++i) {
        const auto byte = static_cast<unsigned char>(text[offset++]);
        if ((byte & 0xc0) != 0x80) throw portus::JsonError("invalid UTF-8");
        value = (value << 6) | (byte & 0x3f);
    }
    if ((count == 2 && value < 0x800) || (count == 3 && value < 0x10000) || value > 0x10ffff
        || (!allow_surrogates && value >= 0xd800 && value <= 0xdfff)) throw portus::JsonError("invalid UTF-8");
    return value;
}

std::u16string sort_key(std::string_view text) {
    std::u16string out;
    for (std::size_t i = 0; i < text.size();) {
        const auto cp = next_codepoint(text, i, true);
        if (cp >= 0x10000) out.push_back(static_cast<char16_t>(0xd800 + ((cp - 0x10000) >> 10)));
        out.push_back(static_cast<char16_t>(cp < 0x10000 ? cp : 0xdc00 + ((cp - 0x10000) & 0x3ff)));
    }
    return out;
}

void quote(std::string& out, std::string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    out += '"';
    for (std::size_t i = 0; i < text.size();) {
        const std::size_t start = i;
        const auto cp = next_codepoint(text, i, true);
        switch (cp) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (cp < 0x20 || (cp >= 0xd800 && cp <= 0xdfff)) {
                out += "\\u";
                for (int shift : {12, 8, 4, 0}) out += hex[(cp >> shift) & 0xf];
            } else {
                out.append(text.substr(start, i - start));
            }
        }
    }
    out += '"';
}

void append(std::string& out, const portus::Json& value, lorica::input_budget& budget, std::size_t depth) {
    if (!budget.permits_depth(depth) || !budget.consume(0, 1, 0)) throw portus::JsonError("registry JSON budget exceeded");
    if (value.is_number()) {
        const double n = value.as_number();
        if (!std::isfinite(n) || std::trunc(n) != n || std::abs(n) > 9007199254740991.0)
            throw portus::JsonError("registry JSON requires safe integers");
        out += std::to_string(static_cast<std::int64_t>(n));
    } else if (value.is_string()) {
        quote(out, value.as_string());
    } else if (value.is_array()) {
        out += '[';
        bool comma = false;
        for (const auto& item : value.items()) {
            if (comma) out += ',';
            comma = true;
            append(out, item, budget, depth + 1);
        }
        out += ']';
    } else if (value.is_object()) {
        std::vector<std::pair<std::u16string, const portus::Json::Member*>> members;
        for (const auto& member : value.members()) {
            if (depth != 0 || member.first != "signature") members.emplace_back(sort_key(member.first), &member);
        }
        std::sort(members.begin(), members.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
        out += '{';
        bool comma = false;
        for (const auto& member : members) {
            if (comma) out += ',';
            comma = true;
            quote(out, member.second->first);
            out += ':';
            append(out, member.second->second, budget, depth + 1);
        }
        out += '}';
    } else {
        out += value.dump();
    }
}

unsigned unicode_escape(std::string_view text, std::size_t offset) {
    if (text.size() - offset < 6 || text.substr(offset, 2) != "\\u") return 0;
    unsigned cp = 0;
    for (char c : text.substr(offset + 2, 4)) {
        if (c >= '0' && c <= '9') cp = (cp << 4) | (c - '0');
        else if (c >= 'a' && c <= 'f') cp = (cp << 4) | (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') cp = (cp << 4) | (c - 'A' + 10);
        else return 0;
    }
    return cp;
}

// The Portus parser replaces unpaired surrogates. Preserve those escaped code units privately for JSON.stringify.
std::string preserve_surrogates(std::string_view text) {
    std::string out;
    bool quoted = false;
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] == '"') quoted = !quoted;
        if (!quoted || text[i] != '\\' || i + 1 == text.size()) {
            out += text[i++];
            continue;
        }
        const unsigned cp = unicode_escape(text, i);
        if (cp >= 0xd800 && cp <= 0xdfff) {
            const unsigned next = unicode_escape(text, i + 6);
            if (cp <= 0xdbff && next >= 0xdc00 && next <= 0xdfff) {
                out.append(text.substr(i, 12));
                i += 12;
                continue;
            }
            out += static_cast<char>(0xe0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (cp & 0x3f));
            i += 6;
        } else {
            out.append(text.substr(i, 2));
            i += 2;
        }
    }
    return out;
}

} // namespace

portus::Json parse_document(const std::string& text) {
    lorica::input_budget budget({kDocumentLimit, 0, 1, 24});
    if (!budget.consume(text.size(), 0, 1)) throw portus::JsonError("registry document too large");
    for (std::size_t i = 0; i < text.size();) (void)next_codepoint(text, i, false);
    return portus::Json::parse(preserve_surrogates(text), budget.remaining().depth);
}

std::string signing_payload(const portus::Json& document) {
    lorica::input_budget budget({0, 100000, 0, 24});
    std::string payload;
    append(payload, document, budget, 0);
    return payload;
}

} // namespace ergo::studio::registry
