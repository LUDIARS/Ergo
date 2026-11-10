#pragma once
/// Routes the Studio server's requests to the loaded plugins:
///   GET /api/health, GET /api/plugins     -> health_report / plugin_list
///   /<id>, /<id>/...                      -> plugin[id].http()  (prefix stripped)
///   WS /<id>/ws                           -> plugin[id].socket()
/// Owns the plugins for the lifetime of the server.
#include "ergo/studio/host/plugin_loader.h"
#include "ergo/studio/net/server.h"

#include <map>
#include <vector>

namespace ergo::studio::host {

class PluginHost final : public net::IHttpHandler, public net::IWsAcceptor {
public:
    PluginHost(LoadResult loaded, LogFn log);

    net::HttpResponse handle(const net::HttpRequest& req) override;
    bool              accepts(const std::string& plugin_id) const override;
    net::WsHandlers   accept(const std::string& plugin_id, const net::HttpRequest& req,
                             std::shared_ptr<net::IWsConnection> conn) override;

    /// Mounted plugins in load order (the shell builds its tabs from these).
    const std::vector<IPlugin*>&        plugins() const { return order_; }
    const std::vector<RejectedPlugin>&  rejected() const { return rejected_; }

private:
    std::vector<LoadedPlugin>        owned_;
    std::vector<IPlugin*>            order_;
    std::map<std::string, IPlugin*>  by_id_;
    std::vector<RejectedPlugin>      rejected_;
    LogFn                            log_;
};

} // namespace ergo::studio::host
