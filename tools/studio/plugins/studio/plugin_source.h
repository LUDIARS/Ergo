#pragma once
/// Plugin source spec and checks (port of studio/domain/plugin-source.ts,
/// ST-INV-08). Spec: `owner/repo:subpath[@rev]`, e.g.
/// `LUDIARS/AdventureCube:tools/ac-web/plugins@main`.
///   - owner must be allow-listed (default LUDIARS): packs run in-process.
///   - subpath is relative to the repo; "..", absolute paths and "\" refused.
///   - rev defaults to HEAD and is resolved to a 40-hex sha before fetching.
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::studio {

struct PluginSource {
    std::string owner, repo, subpath, rev;
};

struct PluginSourceParse {
    std::optional<PluginSource> source;
    std::string                 reason;
};

std::optional<std::string> normalize_subpath(const std::string& raw);
PluginSourceParse          parse_plugin_source(const std::string& spec, const std::vector<std::string>& allowed_owners);
bool                       is_full_sha(const std::string& s);
/// "owner/repo:subpath".
std::string                pack_id(const PluginSource& s);

} // namespace ergo::studio::plugins::studio
