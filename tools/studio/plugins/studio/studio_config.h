#pragma once
/// Studio settings read once from the environment (port of studio/config.ts).
/// No service URL lives here: Excubitor's topology is the source (ST-INV-01);
/// only Curare may be pinned with ERGO_CURARE_URL (a remote Cu Server).
/// Tokens come from the environment and are never stored, shown or logged
/// (ST-INV-09).
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::studio {

/// Environment lookup ("" when unset), injectable for tests.
using EnvFn = std::function<std::string(const std::string&)>;

struct StudioConfig {
    std::string                home;           ///< ERGO_STUDIO_HOME or <user home>/.ergo-studio
    std::optional<std::string> ludiars_root;   ///< LUDIARS_ROOT
    std::optional<std::string> excubitor_url;  ///< EXCUBITOR_URL (else from the catalog)
    std::optional<std::string> actio_token;    ///< ERGO_ACTIO_TOKEN; none = Actio local mode
    std::optional<std::string> curare_url;     ///< ERGO_CURARE_URL
    std::optional<std::string> curare_token;   ///< ERGO_CURARE_TOKEN
    std::vector<std::string>   plugin_owners;  ///< ERGO_PLUGIN_OWNERS (comma list), default {"LUDIARS"}
    std::string                triggered_by = "ergo-studio";
    int                        request_timeout_ms = 8000;   ///< ERGO_STUDIO_TIMEOUT_MS

    const char* actio_mode() const { return actio_token ? "token" : "local"; }
};

/// `user_home` is the OS home directory (USERPROFILE / HOME), "" when unknown;
/// then ERGO_STUDIO_HOME is required for `home` (left empty otherwise).
StudioConfig load_studio_config(const EnvFn& env, const std::string& user_home);

} // namespace ergo::studio::plugins::studio
