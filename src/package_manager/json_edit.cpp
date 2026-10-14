#include "ergo/package_manager/json_edit.h"

#include "json_scan.h"

#include <cstdio>
#include <functional>

namespace ergo::package_manager {

namespace {

using detail::JsonContainer;

/// 足す値の書き方。 multiline のとき member_indent はメンバの行の字下げ、 unit は 1 段。
using Render = std::function<std::string(bool multiline, const std::string& member_indent,
                                         const std::string& unit, const std::string& nl)>;

std::size_t entry_start(const JsonContainer& c, std::size_t i) {
    return c.object ? c.entries[i].key_begin : c.entries[i].value_begin;
}

/// コンテナの末尾に要素を足す。 prefix はメンバなら "\"key\": "、 配列なら ""。
/// prefer_block は空のコンテナを複数行に開くか。
bool insert_entry(std::string& t, const JsonContainer& c, const std::string& prefix,
                  bool prefer_block, const Render& render) {
    const std::string nl   = detail::newline_of(t);
    const std::string unit = detail::indent_unit(t);
    const std::size_t n    = c.entries.size();
    if (n == 0) {
        const char open  = c.object ? '{' : '[';
        const char close = c.object ? '}' : ']';
        std::string body;
        if (prefer_block) {
            const std::string base   = detail::line_indent(t, c.open);
            const std::string member = base + unit;
            body = std::string(1, open) + nl + member + prefix + render(true, member, unit, nl) + nl + base +
                   close;
        } else if (c.object) {
            body = std::string("{ ") + prefix + render(false, "", unit, nl) + " }";
        } else {
            body = std::string("[") + render(false, "", unit, nl) + "]";
        }
        t.replace(c.open, c.close - c.open + 1, body);
        return true;
    }
    std::string sep;
    if (n >= 2) {
        const std::size_t comma = t.find(',', c.entries[n - 2].value_end);
        sep = t.substr(comma + 1, entry_start(c, n - 1) - comma - 1);
    } else {
        sep = t.substr(c.open + 1, entry_start(c, 0) - c.open - 1);
        if (sep.find('\n') == std::string::npos) sep = " ";
    }
    const std::size_t last_nl   = sep.rfind('\n');
    const bool        multiline = last_nl != std::string::npos;
    const std::string member    = multiline ? sep.substr(last_nl + 1) : std::string();
    t.insert(c.entries[n - 1].value_end, "," + sep + prefix + render(multiline, member, unit, nl));
    return true;
}

void erase_entry(std::string& t, const JsonContainer& c, std::size_t i) {
    const std::size_t n = c.entries.size();
    if (n == 1) {
        t.erase(c.open + 1, c.close - c.open - 1);
    } else if (i + 1 < n) {
        t.erase(entry_start(c, i), entry_start(c, i + 1) - entry_start(c, i));
    } else {
        t.erase(c.entries[n - 2].value_end, c.entries[n - 1].value_end - c.entries[n - 2].value_end);
    }
}

Render scalar(const std::string& literal) {
    return [literal](bool, const std::string&, const std::string&, const std::string&) { return literal; };
}

} // namespace

std::string json_quote(const std::string& value) {
    std::string out = "\"";
    for (const char ch : value) {
        const auto c = static_cast<unsigned char>(ch);
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += ch;
                }
        }
    }
    return out + "\"";
}

bool json_has(const std::string& text, const JsonPath& path) {
    std::size_t b = 0;
    std::size_t e = 0;
    return detail::locate_value(text, path, b, e);
}

bool json_replace(std::string& text, const JsonPath& path, const std::string& literal) {
    std::size_t b = 0;
    std::size_t e = 0;
    if (!detail::locate_value(text, path, b, e)) return false;
    text.replace(b, e - b, literal);
    return true;
}

bool json_set_member(std::string& text, const JsonPath& object_path, const std::string& key,
                     const std::string& literal) {
    JsonPath path = object_path;
    path.push_back(key);
    if (json_has(text, path)) return json_replace(text, path, literal);
    JsonContainer c;
    if (!detail::locate_container(text, object_path, c) || !c.object) return false;
    return insert_entry(text, c, json_quote(key) + ": ", false, scalar(literal));
}

bool json_add_object_member(std::string& text, const JsonPath& object_path, const std::string& key,
                            const std::vector<std::pair<std::string, std::string>>& fields) {
    JsonContainer c;
    if (!detail::locate_container(text, object_path, c) || !c.object) return false;
    for (const auto& e : c.entries) {
        if (e.key == key) return false;
    }
    const Render render = [&fields](bool multiline, const std::string& member, const std::string& unit,
                                    const std::string& nl) {
        std::string out = multiline ? "{" + nl : "{ ";
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (multiline) out += member + unit;
            out += json_quote(fields[i].first) + ": " + fields[i].second;
            if (i + 1 < fields.size()) out += multiline ? "," + nl : ", ";
        }
        return out + (multiline ? nl + member + "}" : " }");
    };
    return insert_entry(text, c, json_quote(key) + ": ", true, render);
}

bool json_remove_member(std::string& text, const JsonPath& object_path, const std::string& key) {
    JsonContainer c;
    if (!detail::locate_container(text, object_path, c) || !c.object) return false;
    for (std::size_t i = 0; i < c.entries.size(); ++i) {
        if (c.entries[i].key == key) {
            erase_entry(text, c, i);
            return true;
        }
    }
    return true;
}

bool json_append_string(std::string& text, const JsonPath& array_path, const std::string& value) {
    JsonContainer c;
    if (!detail::locate_container(text, array_path, c) || c.object) return false;
    const std::string literal = json_quote(value);
    for (const auto& e : c.entries) {
        if (text.compare(e.value_begin, e.value_end - e.value_begin, literal) == 0) return true;
    }
    return insert_entry(text, c, "", false, scalar(literal));
}

bool json_remove_string(std::string& text, const JsonPath& array_path, const std::string& value) {
    JsonContainer c;
    if (!detail::locate_container(text, array_path, c) || c.object) return false;
    const std::string literal = json_quote(value);
    for (std::size_t i = 0; i < c.entries.size(); ++i) {
        const auto& e = c.entries[i];
        if (text.compare(e.value_begin, e.value_end - e.value_begin, literal) == 0) {
            erase_entry(text, c, i);
            return true;
        }
    }
    return true;
}

} // namespace ergo::package_manager
