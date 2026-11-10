#pragma once
/// Disk store of Pictor pipeline profiles (*.profile.json, the source of
/// truth Pictor loads with register_presets_from_dir()). Port of
/// render_pipeline/profile_store.ts. The directory comes from
/// ERGO_PICTOR_PROFILE_DIR only: the TS store also guessed a sibling
/// Pictor checkout and then <cwd>/profiles, which this port does not do.
#include "../common/json_util.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ergo::studio::plugins::render_pipeline {

struct ProfileListEntry {
    std::string file;           ///< "standard.profile.json"
    std::string profile_name;   ///< from the file, else the file stem
    uintmax_t   size = 0;
    std::string mtime;
};
JsonValue to_json_value(const ProfileListEntry& e);

/// Only a bare "<name>.profile.json" (no separators, no "..").
bool is_safe_profile_file(const std::string& file);

class ProfileStore {
public:
    /// `dir` empty = not configured.
    explicit ProfileStore(std::filesystem::path dir) : dir_(std::move(dir)) {}

    const std::filesystem::path& dir() const { return dir_; }
    bool                         configured() const { return !dir_.empty(); }

    std::vector<ProfileListEntry> list() const;   ///< sorted by file name

    struct Result {
        bool        ok     = false;
        int         status = 200;
        std::string err;
        std::string file;
        JsonValue   profile;   ///< normalised
        std::string mtime;
    };
    Result load(const std::string& file) const;
    /// Normalise and write. `file` empty = derived from profile_name.
    Result save(const JsonValue& profile, const std::string& file) const;

private:
    std::filesystem::path dir_;
};

} // namespace ergo::studio::plugins::render_pipeline
