#pragma once
/// "rive" plugin: hub for the metadata of the .riv file an editor has open
/// ({name, size, artboards:[...]}) so other clients can follow it. Playback
/// itself is the shell view's job (rive-runtime, design J8).
///   WS in : {op:"publish", meta:{...}} | {op:"ping"}
///   WS out: {op:"meta", meta:{...}|null, clients} | {op:"ack"}
#include "../common/client_set.h"
#include "../common/json_util.h"

#include "ergo/studio/host/plugin.h"

#include <atomic>
#include <mutex>

namespace ergo::studio::plugins::rive {

class RivePlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginSocket,
                         public host::IPluginHealth {
public:
    RivePlugin();

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginSocket*    socket() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    net::WsHandlers   on_open(const net::HttpRequest& req, std::shared_ptr<net::IWsConnection> conn) override;
    JsonValue         snapshot() const override;

    // ---- In-process API (the player view publishes what it opened) ----
    JsonValue meta() const;   ///< null when nothing is open
    /// False when `meta` is not an object.
    bool      publish(const JsonValue& meta);
    uint64_t  revision() const { return revision_.load(); }

private:
    void broadcast();

    host::PluginInfo      info_;
    ClientSet             clients_;
    mutable std::mutex    mtx_;
    JsonValue             meta_;
    std::atomic<uint64_t> revision_{0};
};

std::unique_ptr<host::IPlugin> make_rive_plugin(const host::LogFn& log);

} // namespace ergo::studio::plugins::rive
