#pragma once
/// Melpomene settings (port of Ars-Musa melpomene models/config.rs):
/// <home>/.ars/melpomene/settings.json, the GitHub token in the OS
/// credential store (service "ars-melpomene", account "github_token" — the
/// entry the Tauri app wrote). The token is never written to the JSON; a
/// plain-text token left by an old version is moved to the store on load.
/// Unlike the original, a broken settings file is reported instead of being
/// replaced by defaults silently.
#include "../common/json_util.h"

#include "ergo/studio/platform/credential_store.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::musa_melpomene {

extern const char* const kKeyringService;   ///< "ars-melpomene"
extern const char* const kKeyringAccount;   ///< "github_token"

struct MelpomeneConfig {
    std::string              repository_owner;
    std::string              repository_name;
    std::vector<std::string> default_labels{"melpomene", "auto-generated"};
    std::string              default_priority = "Medium";
    std::string              default_category = "Bug";
    uint64_t                 cache_duration_minutes = 10;
    std::optional<std::string> github_token;   ///< never serialised

    bool        is_configured() const { return !repository_owner.empty() && !repository_name.empty(); }
    /// https://api.github.com/repos/<owner>/<name>
    std::string api_base_url() const;
};

/// GitHub owner / repository names: [A-Za-z0-9._-]+ (anything else would
/// change the URL path).
bool is_valid_repo_part(const std::string& s);

/// The JSON without the token. `has_token` tells the editor whether one is stored.
JsonValue to_json_value(const MelpomeneConfig& c, bool include_has_token = true);
/// Fields present in `v` override `base`; "github_token" is taken when present.
std::optional<MelpomeneConfig> merge_config(const MelpomeneConfig& base, const JsonValue& v, std::string& err);

std::filesystem::path settings_path(const std::string& home);

class ConfigStore {
public:
    ConfigStore(std::filesystem::path file, std::shared_ptr<platform::ICredentialStore> creds);

    struct Loaded {
        MelpomeneConfig config;
        std::string     error;   ///< "" unless the file is broken / unreadable or the store failed
    };
    Loaded load() const;
    bool   save(const MelpomeneConfig& c, std::string& err) const;
    const std::filesystem::path& file() const { return file_; }

private:
    std::filesystem::path                       file_;
    std::shared_ptr<platform::ICredentialStore> creds_;
};

} // namespace ergo::studio::plugins::musa_melpomene
