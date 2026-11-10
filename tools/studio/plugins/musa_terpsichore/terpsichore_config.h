#pragma once
/// Terpsichore command-server settings (port of Ars-Musa terpsichore
/// models/config.rs), stored as <home>/.ars/terpsichore/config.json.
/// Unlike the Rust original there is no built-in port and no silent fallback:
/// a missing file means "not configured", a broken one is reported.
#include "../common/json_util.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace ergo::studio::plugins::musa_terpsichore {

enum class ServerRole { Worker, Watcher, Debugger };
const char*               role_name(ServerRole r);
std::optional<ServerRole> parse_role(const std::string& s);

struct TerpsichoreConfig {
    uint16_t    port    = 0;            ///< required; 0 = unset
    bool        enabled = true;
    std::string token;                  ///< empty = no auth
    ServerRole  role    = ServerRole::Worker;
    std::string host    = "127.0.0.1";  ///< numeric bind address

    bool requires_auth() const { return !token.empty(); }
};

/// Parse a config document. nullopt with `err` for invalid JSON, a missing /
/// out-of-range port or an unknown role.
std::optional<TerpsichoreConfig> parse_config(const JsonValue& v, std::string& err);
JsonValue                        to_json_value(const TerpsichoreConfig& c);

/// <home>/.ars/terpsichore/config.json, empty path when `home` is empty.
std::filesystem::path config_path(const std::string& home);

struct LoadedConfig {
    std::optional<TerpsichoreConfig> config;   ///< nullopt: missing or broken
    std::string                      error;    ///< "" when the file is simply missing
};
LoadedConfig load_config(const std::filesystem::path& file);
bool         save_config(const std::filesystem::path& file, const TerpsichoreConfig& c, std::string& err);

} // namespace ergo::studio::plugins::musa_terpsichore
