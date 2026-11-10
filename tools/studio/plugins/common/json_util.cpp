#include "json_util.h"

namespace ergo::studio::plugins {

namespace jm = ergo::common::jsonm;

std::optional<JsonValue> parse_message(const std::string& text) {
    JsonValue v;
    if (!jm::parse(text, v) || !v.is_object()) return std::nullopt;
    const JsonValue* op = v.find("op");
    if (!op || !op->is_string()) return std::nullopt;
    return v;
}

std::string op_of(const JsonValue& msg) {
    const JsonValue* op = field(msg, "op");
    return op ? op->as_string() : std::string();
}

JsonValue jstr(std::string s) { return JsonValue::make_string(std::move(s)); }
JsonValue jnum(double n) { return JsonValue::make_number(n); }
JsonValue jbool(bool b) { return JsonValue::make_bool(b); }

JsonValue jobj(std::initializer_list<std::pair<std::string, JsonValue>> fields) {
    JsonValue o = JsonValue::make_object();
    for (const auto& [k, v] : fields) o.set(k, v);
    return o;
}

const JsonValue* field(const JsonValue& v, const std::string& key) { return v.is_object() ? v.find(key) : nullptr; }

std::string to_json(const JsonValue& v) { return jm::serialize(v); }

net::HttpResponse json_response(int status, const JsonValue& v) { return net::HttpResponse::json(status, to_json(v)); }

JsonValue deep_copy(const JsonValue& v) {
    JsonValue out = v;
    if (v.is_array()) {
        out = JsonValue::make_array();
        for (const auto& e : *v.a) out.push(deep_copy(e));
    } else if (v.is_object()) {
        out = JsonValue::make_object();
        for (const auto& [k, e] : *v.o) out.set(k, deep_copy(e));
    }
    return out;
}

} // namespace ergo::studio::plugins
