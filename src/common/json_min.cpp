#include "ergo/common/json_min.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace ergo::common::jsonm {

JsonValue JsonValue::make_null()                    { return {}; }
JsonValue JsonValue::make_bool(bool v)              { JsonValue r; r.kind = JsonKind::Bool;   r.b = v; return r; }
JsonValue JsonValue::make_number(double v)          { JsonValue r; r.kind = JsonKind::Number; r.n = v; return r; }
JsonValue JsonValue::make_string(std::string v)     { JsonValue r; r.kind = JsonKind::String; r.s = std::move(v); return r; }
JsonValue JsonValue::make_array()                   { JsonValue r; r.kind = JsonKind::Array;  r.a = std::make_shared<JsonArray>();  return r; }
JsonValue JsonValue::make_object()                  { JsonValue r; r.kind = JsonKind::Object; r.o = std::make_shared<JsonObject>(); return r; }

JsonValue& JsonValue::set(const std::string& key, JsonValue v) {
    if (kind != JsonKind::Object) { kind = JsonKind::Object; o = std::make_shared<JsonObject>(); }
    (*o)[key] = std::move(v);
    return (*o)[key];
}

JsonValue& JsonValue::push(JsonValue v) {
    if (kind != JsonKind::Array) { kind = JsonKind::Array; a = std::make_shared<JsonArray>(); }
    a->push_back(std::move(v));
    return a->back();
}

const JsonValue* JsonValue::find(const std::string& key) const {
    if (kind != JsonKind::Object || !o) return nullptr;
    auto it = o->find(key);
    return it == o->end() ? nullptr : &it->second;
}

namespace {

struct Cursor {
    const std::string& src;
    std::size_t        i   = 0;
    bool               err = false;

    void skip_ws() {
        while (i < src.size()) {
            char c = src[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i; else break;
        }
    }
    bool eof() const { return i >= src.size(); }
    char peek() const { return src[i]; }
    char consume() { return src[i++]; }
    bool match(char c) { skip_ws(); if (eof() || peek() != c) return false; ++i; return true; }
};

// Cap recursion; a hostile payload with thousands of nested brackets would
// otherwise blow the thread stack.
constexpr int kMaxDepth = 64;

bool parse_value(Cursor& c, JsonValue& out, int depth);

bool parse_string(Cursor& c, std::string& out) {
    if (!c.match('"')) { c.err = true; return false; }
    out.clear();
    while (!c.eof()) {
        char ch = c.consume();
        if (ch == '"') return true;
        if (ch == '\\') {
            if (c.eof()) { c.err = true; return false; }
            char esc = c.consume();
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    if (c.src.size() - c.i < 4) { c.err = true; return false; }
                    char hex[5] = {c.src[c.i], c.src[c.i+1], c.src[c.i+2], c.src[c.i+3], 0};
                    c.i += 4;
                    unsigned long cp = std::strtoul(hex, nullptr, 16);
                    if (cp < 0x80) out.push_back(static_cast<char>(cp));
                    else if (cp < 0x800) {
                        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    } else {
                        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default: c.err = true; return false;
            }
        } else {
            out.push_back(ch);
        }
    }
    c.err = true; return false;
}

bool parse_number(Cursor& c, double& out) {
    std::size_t start = c.i;
    if (!c.eof() && (c.peek() == '-' || c.peek() == '+')) ++c.i;
    while (!c.eof() && (std::isdigit(static_cast<unsigned char>(c.peek())) ||
                        c.peek() == '.' || c.peek() == 'e' || c.peek() == 'E' ||
                        c.peek() == '-' || c.peek() == '+')) ++c.i;
    if (start == c.i) { c.err = true; return false; }
    char* endp = nullptr;
    out = std::strtod(c.src.c_str() + start, &endp);
    if (endp == c.src.c_str() + start) { c.err = true; return false; }
    return true;
}

bool parse_array(Cursor& c, JsonValue& out, int depth) {
    out = JsonValue::make_array();
    if (c.match(']')) return true;
    while (true) {
        JsonValue elt;
        if (!parse_value(c, elt, depth)) return false;
        out.push(std::move(elt));
        c.skip_ws();
        if (c.match(',')) continue;
        if (c.match(']')) return true;
        c.err = true; return false;
    }
}

bool parse_object(Cursor& c, JsonValue& out, int depth) {
    out = JsonValue::make_object();
    if (c.match('}')) return true;
    while (true) {
        c.skip_ws();
        std::string key;
        if (!parse_string(c, key)) return false;
        if (!c.match(':')) { c.err = true; return false; }
        JsonValue v;
        if (!parse_value(c, v, depth)) return false;
        out.set(key, std::move(v));
        c.skip_ws();
        if (c.match(',')) continue;
        if (c.match('}')) return true;
        c.err = true; return false;
    }
}

bool parse_value(Cursor& c, JsonValue& out, int depth) {
    if (depth > kMaxDepth) { c.err = true; return false; }
    c.skip_ws();
    if (c.eof()) { c.err = true; return false; }
    char ch = c.peek();
    if (ch == '"') {
        std::string s;
        if (!parse_string(c, s)) return false;
        out = JsonValue::make_string(std::move(s));
        return true;
    }
    if (ch == '{') { ++c.i; return parse_object(c, out, depth + 1); }
    if (ch == '[') { ++c.i; return parse_array(c, out, depth + 1); }
    if (ch == 't') {
        if (c.src.compare(c.i, 4, "true") != 0) { c.err = true; return false; }
        c.i += 4; out = JsonValue::make_bool(true); return true;
    }
    if (ch == 'f') {
        if (c.src.compare(c.i, 5, "false") != 0) { c.err = true; return false; }
        c.i += 5; out = JsonValue::make_bool(false); return true;
    }
    if (ch == 'n') {
        if (c.src.compare(c.i, 4, "null") != 0) { c.err = true; return false; }
        c.i += 4; out = JsonValue::make_null(); return true;
    }
    if (ch == '-' || (ch >= '0' && ch <= '9')) {
        double n;
        if (!parse_number(c, n)) return false;
        out = JsonValue::make_number(n);
        return true;
    }
    c.err = true; return false;
}

void escape_string(const std::string& s, std::string& out) {
    out.push_back('"');
    for (char ch : s) {
        switch (ch) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", ch & 0xFF);
                    out += buf;
                } else out.push_back(ch);
        }
    }
    out.push_back('"');
}

void write_value(const JsonValue& v, std::string& out) {
    switch (v.kind) {
        case JsonKind::Null:   out += "null"; return;
        case JsonKind::Bool:   out += (v.b ? "true" : "false"); return;
        case JsonKind::Number: {
            char buf[64]; std::snprintf(buf, sizeof(buf), "%.17g", v.n); out += buf; return;
        }
        case JsonKind::String: escape_string(v.s, out); return;
        case JsonKind::Array: {
            out.push_back('[');
            if (v.a) { bool first = true;
                for (const auto& e : *v.a) { if (!first) out.push_back(','); write_value(e, out); first = false; }
            }
            out.push_back(']'); return;
        }
        case JsonKind::Object: {
            out.push_back('{');
            if (v.o) { bool first = true;
                for (const auto& [k, val] : *v.o) {
                    if (!first) out.push_back(',');
                    escape_string(k, out); out.push_back(':'); write_value(val, out);
                    first = false;
                }
            }
            out.push_back('}'); return;
        }
    }
}

} // namespace

bool parse(const std::string& src, JsonValue& out) {
    Cursor c{src};
    if (!parse_value(c, out, 0)) return false;
    c.skip_ws();
    return !c.err;
}

std::string serialize(const JsonValue& v) {
    std::string out; out.reserve(64);
    write_value(v, out);
    return out;
}

} // namespace ergo::common::jsonm
