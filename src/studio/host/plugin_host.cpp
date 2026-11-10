#include "ergo/studio/host/plugin_host.h"

#include "ergo/studio/host/health_report.h"

#include <utility>

namespace ergo::studio::host {
namespace {

net::HttpResponse not_found() { return net::HttpResponse::json(404, "{\"ok\":false,\"err\":\"not found\"}"); }
net::HttpResponse not_allowed() { return net::HttpResponse::json(405, "{\"ok\":false,\"err\":\"method not allowed\"}"); }

/// "/<id>/rest" -> {"<id>", "/rest"}; "/<id>" -> {"<id>", "/"}.
std::pair<std::string, std::string> split_plugin_path(const std::string& path) {
    if (path.size() < 2 || path[0] != '/') return {};
    const auto slash = path.find('/', 1);
    if (slash == std::string::npos) return {path.substr(1), "/"};
    return {path.substr(1, slash - 1), path.substr(slash)};
}

} // namespace

PluginHost::PluginHost(LoadResult loaded, LogFn log)
    : owned_(std::move(loaded.plugins)), rejected_(std::move(loaded.rejected)), log_(std::move(log)) {
    for (auto& lp : owned_) {
        IPlugin* p = lp.plugin.get();
        order_.push_back(p);
        by_id_.emplace(p->info().id, p);
    }
}

net::HttpResponse PluginHost::handle(const net::HttpRequest& req) {
    if (req.path == "/api/health") {
        if (req.method != "GET") return not_allowed();
        return net::HttpResponse::json(200, health_report(order_, rejected_));
    }
    if (req.path == "/api/plugins") {
        if (req.method != "GET") return not_allowed();
        return net::HttpResponse::json(200, plugin_list(order_));
    }
    const auto [id, sub] = split_plugin_path(req.path);
    const auto it = by_id_.find(id);
    if (it == by_id_.end()) return not_found();
    IPluginHttp* http = it->second->http();
    if (!http) return not_found();
    return http->handle(req, sub);
}

bool PluginHost::accepts(const std::string& plugin_id) const {
    const auto it = by_id_.find(plugin_id);
    return it != by_id_.end() && it->second->socket() != nullptr;
}

net::WsHandlers PluginHost::accept(const std::string& plugin_id, const net::HttpRequest& req,
                                   std::shared_ptr<net::IWsConnection> conn) {
    // The server calls accept() only after accepts() returned true.
    return by_id_.at(plugin_id)->socket()->on_open(req, std::move(conn));
}

} // namespace ergo::studio::host
