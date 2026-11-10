#pragma once
/// "render_pipeline" plugin: edit Pictor *.profile.json as one node graph
/// (spec/feature/tool/render_pipeline_system_b.md). Routes as in the TS plugin:
///   GET  /api/profiles          GET /api/profile/<file>
///   POST /api/profile {profile, file?}      GET /api/health
///   WS   /render_pipeline/ws  -> {op:"profiles-changed", file} on save;
///        relays {op:"timing", ...} (GPU timestamps) to every client
#include "../common/client_set.h"
#include "profile_store.h"

#include "ergo/studio/host/plugin.h"

#include <atomic>

namespace ergo::studio::plugins::render_pipeline {

class RenderPipelinePlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginSocket,
                                   public host::IPluginHealth {
public:
    RenderPipelinePlugin(std::filesystem::path profile_dir, host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginSocket*    socket() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    net::WsHandlers   on_open(const net::HttpRequest& req, std::shared_ptr<net::IWsConnection> conn) override;
    JsonValue         snapshot() const override;

    // ---- In-process editor API ----
    const ProfileStore&  store() const { return store_; }
    ProfileStore::Result save(const JsonValue& profile, const std::string& file);
    /// Last {op:"timing"} frame relayed (null before the first).
    JsonValue            last_timing() const;
    uint64_t             revision() const { return revision_.load(); }

private:
    host::PluginInfo      info_;
    ProfileStore          store_;
    host::LogFn           log_;
    ClientSet             clients_;
    mutable std::mutex    mtx_;
    JsonValue             last_timing_;
    std::atomic<uint64_t> revision_{0};
};

/// `profile_dir_env` is the raw ERGO_PICTOR_PROFILE_DIR value ("" = unset).
std::unique_ptr<host::IPlugin> make_render_pipeline_plugin(const std::string& profile_dir_env, const host::LogFn& log);

} // namespace ergo::studio::plugins::render_pipeline
