#pragma once
/// Versions of a running game's tuning values (every ergo_bind variable of one
/// app), kept as one JSON file each:
///   {"app": "amicii-server", "name": "...", "note": "...", "created": "<ISO 8601 UTC>",
///    "values": {"balance.walk_mm_per_tick": 60, ...}}
/// in the app's folder: the workspace profile's `tuning` entry for the app, else
/// <studio home>/tuning/<app>. A version is never rewritten: saving again makes
/// a new file (<yyyymmdd-hhmmss>-<slug>.json), so the folder is the history and
/// a repository can track it.
#include "../common/json_util.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::tuning {

struct VersionEntry {
    std::string id;        ///< file stem
    std::string name, note, created;
    std::size_t values = 0;
};

/// The values of `after` that differ from `before` (or are missing there):
/// [{name, before, after}] (pure).
JsonValue diff_values(const JsonValue& before, const JsonValue& after);

/// A file-name-safe form of `name` (ASCII letters, digits, '-', '_'; others become '-').
std::string slug_of(const std::string& name);

class TuningStore {
public:
    explicit TuningStore(std::filesystem::path fallback_root) : fallback_root_(std::move(fallback_root)) {}

    /// Where `app`'s versions live (`profile_dir` wins when not empty).
    std::filesystem::path folder(const std::string& app, const std::filesystem::path& profile_dir) const;

    /// Newest first.
    std::vector<VersionEntry> list(const std::filesystem::path& folder) const;
    std::optional<JsonValue> load(const std::filesystem::path& folder, const std::string& id, std::string& err) const;
    /// Writes a new version; returns its id. `created` is the ISO time to record.
    std::optional<std::string> save(const std::filesystem::path& folder, const std::string& app, const std::string& name,
                                    const std::string& note, const JsonValue& values, const std::string& created,
                                    std::string& err) const;

private:
    std::filesystem::path fallback_root_;
};

/// True for an app name or version id the store will use as a path part.
bool safe_part(const std::string& part);

} // namespace ergo::studio::plugins::tuning
