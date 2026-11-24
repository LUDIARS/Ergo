#pragma once

#include <portus/json.hpp>

namespace ergo::studio::registry {

// Canonical JSON as signed by ErgoPlugin. Signature is omitted only at the root.
std::string signing_payload(const portus::Json& document);
portus::Json parse_document(const std::string& text);

} // namespace ergo::studio::registry
