#pragma once
/// What happens to one request between the wire and the Studio's handlers, in
/// order (spec/feature/tool/ergo.md "Web サーバ"):
///   1. the entrance check of its gate (ingress/entrance.h)      -> 403
///   2. the route table (route_table.h)                          -> 404
///   3. on the remote gate, the loopback-only operations
///      (ingress/remote_scope.h)                                 -> 403
///   4. the handler (an exception                                -> 500)
/// Every answer is recorded once (method, path, status, gate, who). No
/// sockets here: the web server feeds it, and tests call it directly.
#include "ergo/studio/ingress/entrance.h"
#include "ergo/studio/net/server.h"
#include "ergo/studio/web/route_table.h"

#include <functional>
#include <optional>
#include <string>

namespace ergo::studio::web {

/// One answered request (`note` = why it was refused, "" otherwise).
using AccessLogFn = std::function<void(const net::HttpRequest& req, int status, const std::string& note)>;

struct WsRefusal {
    int         status;
    std::string reason;
};

class RequestPipeline {
public:
    RequestPipeline(ingress::EntrancePolicy policy, RouteTable routes, net::IHttpHandler& http, net::IWsAcceptor& ws,
                    AccessLogFn access_log);

    /// Answer an HTTP request whose gate is set. Sets req.who.
    net::HttpResponse answer(net::HttpRequest& req) const;

    /// Check a WebSocket handshake. nullopt = open it with ws() for `plugin_id`
    /// (set from the path /<id>/ws). Sets req.who.
    std::optional<WsRefusal> admit_ws(net::HttpRequest& req, std::string& plugin_id) const;

    net::IWsAcceptor& ws() const { return ws_; }

private:
    net::HttpResponse refuse(const net::HttpRequest& req, int status, const std::string& why) const;

    ingress::EntrancePolicy policy_;
    RouteTable              routes_;
    net::IHttpHandler&      http_;
    net::IWsAcceptor&       ws_;
    AccessLogFn             access_log_;
};

/// Plugin id of "/<id>/ws" or "/<id>/ws/", "" otherwise.
std::string ws_plugin_id(const std::string& path);

} // namespace ergo::studio::web
