#pragma once
/// Plain JSON text (a request body, a file) to a value. parse_message() in
/// json_util.h is for WS frames and insists on an "op"; this does not.
#include "json_util.h"

#include <optional>
#include <string>

namespace ergo::studio::plugins {

/// nullopt for malformed JSON.
inline std::optional<JsonValue> parse_json(const std::string& text) {
    JsonValue out;
    if (!ergo::common::jsonm::parse(text, out)) return std::nullopt;
    return out;
}

} // namespace ergo::studio::plugins
