#include "ergo/studio/web/route_table.h"

#include <portus/http.hpp>

#include <algorithm>
#include <utility>

namespace ergo::studio::web {
namespace {

/// Only matching is used: the handlers are the Studio's own (net::IHttpHandler).
portus::Reply unused(portus::RequestContext&) { return portus::Reply(404); }

std::vector<portus::RouteDef> studio_routes() {
    std::vector<portus::RouteDef> r;
    const auto add = [&r](const char* method, const char* path) { r.emplace_back(method, path, portus::Handler(&unused)); };
    add("GET", "/api/health");
    add("GET", "/api/plugins");
    add("GET", "/api/launcher/projects");
    add("POST", "/api/launcher/open");
    add("POST", "/api/launcher/close");
    add("POST", "/api/launcher/add");
    add("POST", "/api/launcher/remove");
    add("GET", "/shell");   // also "/shell/" (the table ignores empty segments)
    add("GET", "/shell/index.html");
    add("GET", "/shell/tela.js");
    add("GET", "/shell/shell.js");
    return r;
}

/// "<id>" of "/<id>" or "/<id>/...", "" otherwise.
std::string first_segment(const std::string& path) {
    if (path.size() < 2 || path[0] != '/') return {};
    const auto slash = path.find('/', 1);
    return slash == std::string::npos ? path.substr(1) : path.substr(1, slash - 1);
}

} // namespace

struct RouteTable::Impl {
    portus::Router           studio{studio_routes()};
    std::vector<std::string> plugins;
};

RouteTable::RouteTable(std::vector<std::string> plugin_ids) : impl_(std::make_unique<Impl>()) {
    impl_->plugins = std::move(plugin_ids);
}

RouteTable::~RouteTable()                                = default;
RouteTable::RouteTable(RouteTable&&) noexcept            = default;
RouteTable& RouteTable::operator=(RouteTable&&) noexcept = default;

bool RouteTable::reaches(const net::HttpRequest& req) const {
    if (impl_->studio.match(req.method, req.target)) return true;
    const std::string id = first_segment(req.path);
    return !id.empty() && std::find(impl_->plugins.begin(), impl_->plugins.end(), id) != impl_->plugins.end();
}

} // namespace ergo::studio::web
