#pragma once
/// "log" plugin: live ergo_log viewer. ergo_log_live engines send hello and
/// batches of lines over /log/ws; each app keeps its newest lines in a ring.
/// Editors get every ring as `backlog`, then live `lines`. Spec:
/// spec/feature/tool/log.md.
#include "../common/client_set.h"
#include "log_apps.h"

#include "ergo/studio/host/plugin.h"

#include <atomic>
#include <map>
#include <mutex>

namespace ergo::studio::plugins::log {

class LogPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginSocket,
                        public host::IPluginHealth {
public:
    explicit LogPlugin(host::LogFn log, std::size_t ring_capacity = kRingCapacity);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginSocket*    socket() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    net::WsHandlers   on_open(const net::HttpRequest& req, std::shared_ptr<net::IWsConnection> conn) override;
    JsonValue         snapshot() const override;

    // ---- In-process viewer API ----
    std::vector<AppInfo>          apps() const;
    std::optional<LogApps::Lines> lines(const std::string& app, const LineFilter& f, std::size_t limit) const;
    uint64_t                      revision() const { return revision_.load(); }

private:
    void on_text(uint64_t id, const std::string& text);
    void on_close(uint64_t id);
    /// False when the engine limit is reached (the connection is closed).
    bool become_engine(uint64_t id, const std::string& requested);
    void broadcast_apps();

    host::PluginInfo              info_;
    host::LogFn                   log_;
    ClientSet                     clients_;
    mutable std::mutex            mtx_;
    LogApps                       apps_;
    std::map<uint64_t, std::string> engine_app_;   // engine connection -> app
    std::map<uint64_t, std::shared_ptr<net::IWsConnection>> conns_;   // for 1013 close
    std::atomic<uint64_t>         revision_{0};
};

std::unique_ptr<host::IPlugin> make_log_plugin(const host::LogFn& log);

} // namespace ergo::studio::plugins::log
