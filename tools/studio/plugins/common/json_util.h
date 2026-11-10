#pragma once
/// Small JSON helpers shared by the built-in plugins.
#include "ergo/common/json_min.h"
#include "ergo/studio/net/http_message.h"

#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

namespace ergo::studio::plugins {

using ergo::common::jsonm::JsonValue;

/// A WS text frame as `{op: string, ...}`; nullopt for anything else
/// (malformed JSON, non-object, missing / non-string op). Callers drop those
/// frames silently, as the TS plugins did.
std::optional<JsonValue> parse_message(const std::string& text);

/// The "op" of a value parse_message() accepted.
std::string op_of(const JsonValue& msg);

JsonValue jstr(std::string s);
JsonValue jnum(double n);
JsonValue jbool(bool b);
JsonValue jobj(std::initializer_list<std::pair<std::string, JsonValue>> fields);

/// Field of an object, or nullptr when `v` is not an object or lacks it.
const JsonValue* field(const JsonValue& v, const std::string& key);

std::string       to_json(const JsonValue& v);
net::HttpResponse json_response(int status, const JsonValue& v);

/// Deep copy (JsonValue shares arrays / objects through shared_ptr).
JsonValue deep_copy(const JsonValue& v);

} // namespace ergo::studio::plugins
