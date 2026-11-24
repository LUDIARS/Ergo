#pragma once
/// The routes the Studio's web server lets reach a handler
/// (spec/feature/tool/ergo.md "Web サーバ"). Only what is written here
/// arrives; anything else is answered 404 before any handler runs:
///   the Studio's own routes, one row each (a Portus route table:
///     GET /api/health, /api/plugins, /api/launcher/projects,
///     POST /api/launcher/open|close|add|remove,
///     GET /shell, /shell/index.html, /shell/tela.js, /shell/shell.js), and
///   the subtree /<id> and /<id>/... of every mounted plugin (its own table).
/// WebSocket handshakes are routed by the acceptor (/<id>/ws), not here.
#include "ergo/studio/net/http_message.h"

#include <memory>
#include <string>
#include <vector>

namespace ergo::studio::web {

class RouteTable {
public:
    /// `plugin_ids` = the plugins mounted under /<id>.
    explicit RouteTable(std::vector<std::string> plugin_ids);
    ~RouteTable();
    RouteTable(RouteTable&&) noexcept;
    RouteTable& operator=(RouteTable&&) noexcept;

    /// True when `req` (method, target, decoded path) is on the table.
    bool reaches(const net::HttpRequest& req) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ergo::studio::web
