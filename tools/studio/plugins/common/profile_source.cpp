#include "profile_source.h"

#include "file_io.h"

namespace ergo::studio::plugins {

namespace fs = std::filesystem;

ProfileSet read_profile_file(const fs::path& file, const ProfileEnvFn& env) {
    ProfileSet out;
    std::string err;
    const auto text = read_text_file(file, err);
    if (!text) {
        out.errors.push_back(file.u8string() + ": " + err);
        return out;
    }
    auto profile = parse_workspace_profile(*text, file, env, err);
    if (!profile) {
        out.errors.push_back(file.u8string() + ": " + err);
        return out;
    }
    out.profiles.push_back(std::move(*profile));
    return out;
}

OpenProjectProfiles::OpenProjectProfiles(const project::OpenProject& open, ProfileEnvFn env)
    : open_(open), env_(std::move(env)) {}

ProfileSet OpenProjectProfiles::profiles() const {
    const auto current = open_.current();
    if (!current) return {};
    return read_profile_file(fs::u8path(current->source), env_);
}

} // namespace ergo::studio::plugins
