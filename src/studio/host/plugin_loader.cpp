#include "ergo/studio/host/plugin_loader.h"

#include <map>
#include <optional>
#include <stdexcept>

namespace ergo::studio::host {
namespace {

const char* const kBuiltinSource = "built-in";

struct Attempt {
    std::unique_ptr<IPlugin> plugin;   // set on success
    std::string              id;
    std::string              reason;
};

std::optional<std::string> info_problem(const PluginInfo& info) {
    if (!is_valid_plugin_id(info.id)) return "invalid id '" + info.id + "' (must match ^[a-z0-9][a-z0-9_-]*$)";
    if (is_reserved_plugin_id(info.id)) return "reserved id '" + info.id + "'";
    if (info.title.empty()) return std::string("title is empty");
    return std::nullopt;
}

Attempt try_instantiate(const PluginFactory& factory, const std::map<std::string, std::string>& owner,
                        const LogFn& log) {
    Attempt a;
    try {
        if (!factory) { a.reason = "factory is empty"; return a; }
        auto p = factory(log);
        if (!p) { a.reason = "factory returned no plugin"; return a; }
        const PluginInfo& info = p->info();
        a.id = info.id;
        if (auto problem = info_problem(info)) { a.reason = *problem; return a; }
        const auto prev = owner.find(info.id);
        if (prev != owner.end()) {
            a.reason = "duplicate id '" + info.id + "' (already provided by " + prev->second + ")";
            return a;
        }
        a.plugin = std::move(p);
    } catch (const std::exception& e) {
        a.reason = std::string("factory threw: ") + e.what();
    } catch (...) {
        a.reason = "factory threw a non-standard exception";
    }
    return a;
}

} // namespace

LoadResult instantiate_plugins(const std::vector<PluginFactory>&   builtins,
                               const std::vector<ExternalFactory>& externals,
                               const LogFn&                        log) {
    LoadResult out;
    std::map<std::string, std::string> owner;   // id -> source

    auto take = [&](const PluginFactory& f, const std::string& source, bool builtin) {
        Attempt a = try_instantiate(f, owner, log);
        if (a.plugin) {
            owner.emplace(a.plugin->info().id, source);
            out.plugins.push_back({std::move(a.plugin), source, builtin});
            return;
        }
        if (builtin) {
            throw std::runtime_error("[ergo] built-in plugin" + (a.id.empty() ? std::string() : " '" + a.id + "'")
                                     + " rejected: " + a.reason);
        }
        if (log) log("error", "[ergo] external plugin " + source + " rejected: " + a.reason);
        out.rejected.push_back({a.id, source, a.reason});
    };

    for (const auto& f : builtins) take(f, kBuiltinSource, true);
    for (const auto& e : externals) take(e.factory, e.source, false);
    return out;
}

} // namespace ergo::studio::host
