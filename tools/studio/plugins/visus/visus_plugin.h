#pragma once
/// "visus" plugin: HTTP API over VisusStore plus save notifications on
/// /visus/ws (live reload). Routes as in the TS plugin:
///   GET /api/roots, GET /api/scan?root=, GET /api/load?root=&path=,
///   POST /api/save {root, path, content}, GET /api/health
#include "../common/client_set.h"
#include "visus_store.h"

#include "ergo/studio/host/plugin.h"

#include <atomic>

namespace ergo::studio::plugins::visus {

class VisusPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginSocket,
                          public host::IPluginHealth {
public:
    VisusPlugin(std::vector<std::filesystem::path> roots, host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginSocket*    socket() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    net::WsHandlers   on_open(const net::HttpRequest& req, std::shared_ptr<net::IWsConnection> conn) override;
    JsonValue         snapshot() const override;

    // ---- In-process editor API ----
    const VisusStore& store() const { return store_; }
    /// Save and notify clients ({op:"saved", root_index, rel_path}).
    VisusStore::Result save(double root_index, const std::string& rel, const std::string& content);
    uint64_t          revision() const { return revision_.load(); }

private:
    host::PluginInfo      info_;
    host::LogFn           log_;
    VisusStore            store_;
    ClientSet             clients_;
    std::atomic<uint64_t> revision_{0};
};

/// `roots_env` is the raw VISUS_PROJECT_ROOTS value ("" = no roots).
std::unique_ptr<host::IPlugin> make_visus_plugin(const std::string& roots_env, const host::LogFn& log);

} // namespace ergo::studio::plugins::visus
