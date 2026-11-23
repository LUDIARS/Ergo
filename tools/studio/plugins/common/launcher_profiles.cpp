#include "launcher_profiles.h"

#include "profile_source.h"

#include "ergo/studio/project/project_style.h"

#include <chrono>
#include <ctime>

namespace ergo::studio::plugins {

namespace fs = std::filesystem;

namespace {

project::ProjectRef ref_of(const WorkspaceProfile& p, const fs::path& source) {
    const std::string style =
        p.style.empty() ? project::style_name(project::detect(project::DiskProbe(p.root))) : p.style;
    return {source.u8string(), p.name, p.root.u8string(), style, p.domain};
}

} // namespace

std::string utc_now_iso() {
    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

project::LauncherDeps launcher_deps(const fs::path& home, const std::string& plugin_dirs, ProfileEnvFn env) {
    project::LauncherDeps deps;
    deps.probe = [env](const fs::path& source, std::string& err) -> std::optional<project::ProjectRef> {
        const ProfileSet set = read_profile_file(source, env);
        if (set.profiles.empty()) {
            err = set.errors.empty() ? source.u8string() + ": not a profile" : set.errors.front();
            return std::nullopt;
        }
        const WorkspaceProfile& p = set.profiles.front();
        return ref_of(p, source);
    };
    deps.discover = [home, plugin_dirs, env] {
        std::vector<project::ProjectRef> out;
        for (const auto& p : load_workspace_profiles(home, plugin_dirs, env).profiles) {
            out.push_back(ref_of(p, p.source));
        }
        return out;
    };
    deps.now = &utc_now_iso;
    return deps;
}

} // namespace ergo::studio::plugins
