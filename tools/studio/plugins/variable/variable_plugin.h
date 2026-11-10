#pragma once
/// "variable" plugin: hub between ergo_bind engines (/variable/ws, role
/// "engine") and editors. Editors are the Studio's own view (in process,
/// through the methods below) or a WS client with role "ui".
#include "../common/client_set.h"
#include "variable_registry.h"

#include "ergo/studio/host/plugin.h"

#include <atomic>
#include <mutex>

namespace ergo::studio::plugins::variable {

class VariablePlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginSocket,
                             public host::IPluginHealth {
public:
    VariablePlugin();

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginSocket*    socket() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    net::WsHandlers   on_open(const net::HttpRequest& req, std::shared_ptr<net::IWsConnection> conn) override;
    JsonValue         snapshot() const override;

    // ---- In-process editor API (the Studio view) ----
    std::vector<BoundVar>  vars() const;
    std::vector<ActorNode> actors() const;
    /// Ask the owning engine to change a variable. False when it is not bound.
    bool request_set(const std::string& name, const JsonValue& value);
    /// Bumped on every registry / value change; the view redraws when it moves.
    uint64_t revision() const { return revision_.load(); }

private:
    void on_text(uint64_t id, const std::string& text);
    void on_close(uint64_t id);
    void on_engine(uint64_t id, const std::string& op, const JsonValue& msg);
    void broadcast_registry();   // call without mtx_ held
    void broadcast_actors();

    host::PluginInfo      info_;
    ClientSet             clients_;
    mutable std::mutex    mtx_;
    VariableRegistry      registry_;
    std::map<uint64_t, std::string> apps_;   // engine connection -> app name
    std::atomic<uint64_t> revision_{0};
};

std::unique_ptr<host::IPlugin> make_variable_plugin(const host::LogFn& log);

} // namespace ergo::studio::plugins::variable
