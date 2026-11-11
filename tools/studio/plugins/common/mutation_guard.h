#pragma once
/// Guard for the Studio's mutating API (port of studio/domain/request-guard.ts,
/// ST-INV-03): a page on another site must not be able to POST to localhost
/// and trigger an AI instruction or a plugin install (the "studio" plugin) or
/// link the Studio to a class (the "lesson" plugin). Non-GET calls need
///   - a loopback Host (DNS rebinding),
///   - an Origin, if any, on the same loopback host:port,
///   - a JSON body (DELETE excepted).
/// Calls without Origin (native tools, tests) are judged by Host only.
#include <optional>
#include <string>

namespace ergo::studio::plugins {

/// nullopt = allowed; otherwise the reason (logged, returned with 403).
std::optional<std::string> check_mutation(const std::string& method, const std::optional<std::string>& host,
                                          const std::optional<std::string>& origin,
                                          const std::optional<std::string>& content_type);

} // namespace ergo::studio::plugins
