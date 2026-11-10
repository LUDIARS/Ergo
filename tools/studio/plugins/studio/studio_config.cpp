#include "studio_config.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>

namespace ergo::studio::plugins::studio {
namespace {

std::optional<std::string> non_empty(const std::string& v) {
    const auto b = v.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::nullopt;
    return v.substr(b, v.find_last_not_of(" \t\r\n") - b + 1);
}

std::optional<std::string> strip_slash(std::optional<std::string> url) {
    if (!url) return url;
    while (!url->empty() && url->back() == '/') url->pop_back();
    return url->empty() ? std::nullopt : url;
}

} // namespace

StudioConfig load_studio_config(const EnvFn& env, const std::string& user_home) {
    StudioConfig c;
    namespace fs = std::filesystem;
    if (const auto h = non_empty(env("ERGO_STUDIO_HOME"))) {
        c.home = fs::absolute(fs::u8path(*h)).lexically_normal().u8string();
    } else if (!user_home.empty()) {
        c.home = (fs::u8path(user_home) / ".ergo-studio").lexically_normal().u8string();
    }
    c.ludiars_root  = non_empty(env("LUDIARS_ROOT"));
    c.excubitor_url = strip_slash(non_empty(env("EXCUBITOR_URL")));
    c.actio_token   = non_empty(env("ERGO_ACTIO_TOKEN"));
    c.curare_url    = strip_slash(non_empty(env("ERGO_CURARE_URL")));
    c.curare_token  = non_empty(env("ERGO_CURARE_TOKEN"));
    if (const auto owners = non_empty(env("ERGO_PLUGIN_OWNERS"))) {
        std::size_t start = 0;
        while (start <= owners->size()) {
            const auto comma = owners->find(',', start);
            if (auto o = non_empty(owners->substr(start, comma == std::string::npos ? std::string::npos : comma - start))) {
                c.plugin_owners.push_back(*o);
            }
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    }
    if (c.plugin_owners.empty()) c.plugin_owners = {"LUDIARS"};
    const std::string t = env("ERGO_STUDIO_TIMEOUT_MS");
    char* e = nullptr;
    const double ms = t.empty() ? 0 : std::strtod(t.c_str(), &e);
    if (!t.empty() && e == t.c_str() + t.size() && std::isfinite(ms) && ms > 0) c.request_timeout_ms = static_cast<int>(ms);
    return c;
}

} // namespace ergo::studio::plugins::studio
