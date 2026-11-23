#pragma once
/// IProfileSource: the workspace profiles a plugin may look at. The game-ops
/// plugins (assets, play, services, tuning, git) take one instead of reading
/// every profile on disk with load_workspace_profiles(), so they see only the
/// project open in the launcher (spec/plan/2026-11-23-studio-launcher-and-devserver.md).
/// Tests give them a fixed set.
#include "workspace_profiles.h"

#include "ergo/studio/project/open_project.h"

#include <memory>

namespace ergo::studio::plugins {

class IProfileSource {
public:
    virtual ~IProfileSource() = default;
    /// The profiles to use now (read on every call: the open project can change).
    virtual ProfileSet profiles() const = 0;
};

/// The open project's profile, read from its file; empty while none is open.
class OpenProjectProfiles final : public IProfileSource {
public:
    /// `open` outlives the source.
    explicit OpenProjectProfiles(const project::OpenProject& open, ProfileEnvFn env = &process_env);
    ProfileSet profiles() const override;

private:
    const project::OpenProject& open_;
    ProfileEnvFn                env_;
};

/// A set given once (tests, tools that read their profiles themselves).
class FixedProfiles final : public IProfileSource {
public:
    explicit FixedProfiles(ProfileSet set) : set_(std::move(set)) {}
    ProfileSet profiles() const override { return set_; }

private:
    ProfileSet set_;
};

/// The profile in `file`, as a set (one profile, or one error).
ProfileSet read_profile_file(const std::filesystem::path& file, const ProfileEnvFn& env);

} // namespace ergo::studio::plugins
