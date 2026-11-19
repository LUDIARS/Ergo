#include "sonus_config.h"

#include <cstdlib>

namespace ergo::studio::plugins::sonus {
namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

/// "" = unset (keeps `out`); false when the text is not an integer in [lo, hi].
bool read_int(const EnvFn& env, const char* key, long lo, long hi, long& out, std::string& error) {
    const std::string v = trim(env(key));
    if (v.empty()) return true;
    char*      e = nullptr;
    const long n = std::strtol(v.c_str(), &e, 10);
    if (e != v.c_str() + v.size() || n < lo || n > hi) {
        if (error.empty()) error = std::string(key) + " must be " + std::to_string(lo) + ".." + std::to_string(hi) + " (got \"" + v + "\")";
        return false;
    }
    out = n;
    return true;
}

} // namespace

SonusConfig load_sonus_config(const EnvFn& env, std::string& error) {
    SonusConfig c;
    error.clear();
    if (const std::string h = trim(env("ERGO_SONUS_HOST")); !h.empty()) c.host = h;
    long port = c.port;
    if (read_int(env, "ERGO_SONUS_PORT", 1, 65535, port, error)) c.port = static_cast<uint16_t>(port);
    long tick = c.tick_ms;
    if (read_int(env, "ERGO_SONUS_TICK_MS", 10, 1000, tick, error)) c.tick_ms = static_cast<int>(tick);
    c.project_dir   = trim(env("ERGO_SONUS_PROJECT"));
    c.build_dir     = trim(env("ERGO_SONUS_BUILD_DIR"));
    c.recording_dir = trim(env("ERGO_SONUS_RECORDING_DIR"));
    if (!c.build_dir.empty() && c.project_dir.empty() && error.empty()) {
        error = "ERGO_SONUS_BUILD_DIR needs ERGO_SONUS_PROJECT (the project to build)";
    }
    return c;
}

} // namespace ergo::studio::plugins::sonus
