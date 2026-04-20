#pragma once

/// Minimal JSON parser / serializer shared by the Ergo WS protocols
/// (inspector, bind, and any future module that needs a wire-format codec
/// for small RPC payloads).
///
/// Scope: objects, arrays, strings, numbers (parsed as double), booleans,
/// null. No streaming, no schema validation. A hostile payload cannot
/// crash the process — parse() fails on malformed input and caps recursion
/// at a fixed depth to avoid stack exhaustion.
///
/// This header lives under `ergo/common/` and is re-exported as
/// `ergo::inspector::jsonm` / `ergo::bind::jsonm` via namespace aliases so
/// existing callers compile unchanged.

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ergo::common::jsonm {

enum class JsonKind : uint8_t { Null, Bool, Number, String, Array, Object };

struct JsonValue;
using JsonArray  = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

struct JsonValue {
    JsonKind                          kind = JsonKind::Null;
    bool                              b    = false;
    double                            n    = 0.0;
    std::string                       s;
    std::shared_ptr<JsonArray>        a;
    std::shared_ptr<JsonObject>       o;

    static JsonValue make_null();
    static JsonValue make_bool(bool v);
    static JsonValue make_number(double v);
    static JsonValue make_string(std::string v);
    static JsonValue make_array();
    static JsonValue make_object();

    JsonValue& set(const std::string& key, JsonValue v);
    JsonValue& push(JsonValue v);

    const JsonValue* find(const std::string& key) const;

    bool is_null()   const { return kind == JsonKind::Null;  }
    bool is_bool()   const { return kind == JsonKind::Bool;  }
    bool is_number() const { return kind == JsonKind::Number;}
    bool is_string() const { return kind == JsonKind::String;}
    bool is_array()  const { return kind == JsonKind::Array; }
    bool is_object() const { return kind == JsonKind::Object;}

    std::string as_string(std::string fallback = {}) const { return is_string() ? s : fallback; }
    double      as_number(double fallback = 0.0) const     { return is_number() ? n : fallback; }
    bool        as_bool(bool fallback = false) const       { return is_bool() ? b : fallback; }
};

bool        parse(const std::string& src, JsonValue& out);
std::string serialize(const JsonValue& v);

} // namespace ergo::common::jsonm
