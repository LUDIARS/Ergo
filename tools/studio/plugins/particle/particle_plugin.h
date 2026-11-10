#pragma once
/// "particle" plugin: mirrors one ParticleEffectConfig to every client of
/// /particle/ws (ergo_particle_link engines receive "state" frames). The
/// Studio view edits it in process through set() / replace().
#include "../common/client_set.h"
#include "particle_schema.h"

#include "ergo/studio/host/plugin.h"

#include <atomic>
#include <mutex>

namespace ergo::studio::plugins::particle {

class ParticlePlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginSocket,
                             public host::IPluginHealth {
public:
    ParticlePlugin();

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginSocket*    socket() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    net::WsHandlers   on_open(const net::HttpRequest& req, std::shared_ptr<net::IWsConnection> conn) override;
    JsonValue         snapshot() const override;

    // ---- In-process editor API ----
    JsonValue config() const;
    /// Merge a partial config and broadcast.
    void      set(const JsonValue& partial);
    /// Replace from the defaults and broadcast. False when `full` is not an object.
    bool      replace(const JsonValue& full);
    uint64_t  revision() const { return revision_.load(); }

private:
    void broadcast_state();

    host::PluginInfo      info_;
    ClientSet             clients_;
    mutable std::mutex    mtx_;
    JsonValue             current_;
    std::atomic<uint64_t> revision_{0};
};

std::unique_ptr<host::IPlugin> make_particle_plugin(const host::LogFn& log);

} // namespace ergo::studio::plugins::particle
