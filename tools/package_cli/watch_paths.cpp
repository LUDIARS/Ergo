#include "watch_paths.h"

#include "file_io.h"

#include <cstdlib>

namespace ergo::package_cli {

const char* const kWatchReportPath = ".ergo/watch-report.json";
const char* const kEventsFile      = "events.json";

namespace {

std::string env(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

std::string first_of(const std::string& a, const char* env_a, const char* env_b = nullptr) {
    if (!a.empty()) return a;
    if (std::string v = env(env_a); !v.empty()) return v;
    return env_b ? env(env_b) : std::string();
}

} // namespace

std::string watch_state_dir(const Args& args) {
    if (!args.state.empty()) return args.state;
#if defined(_WIN32)
    if (std::string base = env("LOCALAPPDATA"); !base.empty()) return join(base, "ergo/pkg-watch");
#else
    if (std::string base = env("XDG_STATE_HOME"); !base.empty()) return join(base, "ergo/pkg-watch");
    if (std::string home = env("HOME"); !home.empty()) return join(home, ".local/state/ergo/pkg-watch");
#endif
    return {};
}

std::string watch_cache_dir(const Args& args) {
    return first_of(args.cache, "ERGO_PKG_CACHE_DIR", "ERGO_DEPENDENCY_CACHE_DIR");
}

std::string watch_sibling_root(const Args& args) {
    return first_of(args.root, "ERGO_PACKAGES_SIBLING_ROOT");
}

std::string state_file(const std::string& state_dir, const std::string& code) {
    return join(state_dir, code + ".watch-state.json");
}

} // namespace ergo::package_cli
