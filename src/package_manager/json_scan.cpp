#include "json_scan.h"

#include "ergo/common/json_min.h"

namespace ergo::package_manager::detail {

namespace {

bool is_ws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

bool scan_string_end(const std::string& t, std::size_t begin, std::size_t& end) {
    for (std::size_t i = begin + 1; i < t.size(); ++i) {
        if (t[i] == '\\') {
            ++i;
        } else if (t[i] == '"') {
            end = i + 1;
            return true;
        }
    }
    return false;
}

bool decode_string(const std::string& t, std::size_t begin, std::size_t end, std::string& out) {
    ergo::common::jsonm::JsonValue v;
    if (!ergo::common::jsonm::parse(t.substr(begin, end - begin), v) || !v.is_string()) return false;
    out = v.s;
    return true;
}

} // namespace

std::size_t skip_ws(const std::string& t, std::size_t i) {
    while (i < t.size() && is_ws(t[i])) ++i;
    return i;
}

bool scan_value_end(const std::string& t, std::size_t begin, std::size_t& end) {
    if (begin >= t.size()) return false;
    const char c = t[begin];
    if (c == '"') return scan_string_end(t, begin, end);
    if (c == '{' || c == '[') {
        int depth = 0;
        for (std::size_t i = begin; i < t.size(); ++i) {
            const char d = t[i];
            if (d == '"') {
                std::size_t s = 0;
                if (!scan_string_end(t, i, s)) return false;
                i = s - 1;
            } else if (d == '{' || d == '[') {
                ++depth;
            } else if (d == '}' || d == ']') {
                if (--depth == 0) {
                    end = i + 1;
                    return true;
                }
            }
        }
        return false;
    }
    std::size_t i = begin;
    while (i < t.size() && !is_ws(t[i]) && t[i] != ',' && t[i] != '}' && t[i] != ']') ++i;
    end = i;
    return i > begin;
}

bool scan_container(const std::string& t, std::size_t open, JsonContainer& out) {
    if (open >= t.size() || (t[open] != '{' && t[open] != '[')) return false;
    out        = JsonContainer{};
    out.object = t[open] == '{';
    out.open   = open;
    const char close = out.object ? '}' : ']';
    std::size_t i = skip_ws(t, open + 1);
    if (i < t.size() && t[i] == close) {
        out.close = i;
        return true;
    }
    while (i < t.size()) {
        JsonEntry e;
        e.key_begin = i;
        if (out.object) {
            std::size_t key_end = 0;
            if (t[i] != '"' || !scan_string_end(t, i, key_end)) return false;
            if (!decode_string(t, i, key_end, e.key)) return false;
            i = skip_ws(t, key_end);
            if (i >= t.size() || t[i] != ':') return false;
            i = skip_ws(t, i + 1);
        }
        e.value_begin = i;
        if (!scan_value_end(t, i, e.value_end)) return false;
        out.entries.push_back(e);
        i = skip_ws(t, e.value_end);
        if (i >= t.size()) return false;
        if (t[i] == close) {
            out.close = i;
            return true;
        }
        if (t[i] != ',') return false;
        i = skip_ws(t, i + 1);
    }
    return false;
}

bool locate_value(const std::string& t, const JsonPath& path, std::size_t& begin, std::size_t& end) {
    std::size_t b = skip_ws(t, 0);
    std::size_t e = 0;
    if (!scan_value_end(t, b, e)) return false;
    for (const auto& key : path) {
        JsonContainer c;
        if (!scan_container(t, b, c) || !c.object) return false;
        bool found = false;
        for (const auto& entry : c.entries) {
            if (entry.key == key) {
                b     = entry.value_begin;
                e     = entry.value_end;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    begin = b;
    end   = e;
    return true;
}

bool locate_container(const std::string& t, const JsonPath& path, JsonContainer& out) {
    std::size_t b = 0;
    std::size_t e = 0;
    return locate_value(t, path, b, e) && scan_container(t, b, out);
}

std::string line_indent(const std::string& t, std::size_t pos) {
    std::size_t start = pos;
    while (start > 0 && t[start - 1] != '\n') --start;
    std::size_t i = start;
    while (i < t.size() && (t[i] == ' ' || t[i] == '\t')) ++i;
    return t.substr(start, i - start);
}

std::string newline_of(const std::string& t) {
    return t.find("\r\n") != std::string::npos ? "\r\n" : "\n";
}

std::string indent_unit(const std::string& t) {
    for (std::size_t i = t.find('\n'); i != std::string::npos; i = t.find('\n', i + 1)) {
        std::size_t j = i + 1;
        while (j < t.size() && (t[j] == ' ' || t[j] == '\t')) ++j;
        if (j > i + 1 && j < t.size() && t[j] != '\r' && t[j] != '\n') return t.substr(i + 1, j - i - 1);
    }
    return "  ";
}

} // namespace ergo::package_manager::detail
