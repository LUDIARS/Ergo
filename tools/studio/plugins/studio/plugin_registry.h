#pragma once

#include "plugin_ledger.h"
#include "ergo/studio/registry/fetch.h"

#include <chrono>
#include <mutex>

namespace ergo::studio::plugins::studio {

class PluginRegistry {
public:
    PluginRegistry(std::filesystem::path home, std::string root, std::unique_ptr<registry::IAssetTransport> transport,
                   int timeout_ms);
    registry::Trust refresh(bool require_fresh = true);
    void filter_startup(Ledger& ledger);

private:
    registry::Trust fetch_and_verify();
    std::filesystem::path home_;
    std::string root_;
    std::unique_ptr<registry::IAssetTransport> transport_;
    int timeout_ms_;
    std::mutex mutex_;
    registry::Trust cached_;
    std::chrono::steady_clock::time_point cached_at_{};
};

std::unique_ptr<PluginRegistry> make_registry(const std::filesystem::path& home, int timeout_ms);
std::string registry_plugin_dirs(const std::string& raw, const std::filesystem::path& home, int timeout_ms);

} // namespace ergo::studio::plugins::studio
