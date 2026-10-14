#include "watch_observe.h"

#include "ergo/package_watch/observation.h"

namespace ergo::package_cli {

std::vector<pw::DependencyResult> observe_host(const WatchHost& host, pw::IRefSource& refs,
                                               pw::IHistorySource& history, const pw::WatchState& previous) {
    std::vector<pw::DependencyResult> out;
    for (const auto& dep : host.dependencies) {
        const auto fp = previous.fingerprints.find(dep.name);
        const std::string prev = fp == previous.fingerprints.end() ? std::string() : fp->second;
        out.push_back(pw::make_result(dep, pw::observe_dependency(dep, refs, history, prev)));
    }
    return out;
}

} // namespace ergo::package_cli
