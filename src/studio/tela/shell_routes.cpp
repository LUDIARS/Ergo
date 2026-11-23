#include "ergo/studio/tela/shell_routes.h"

#include "ergo/studio/net/request_guard.h"

#include <utility>

// Compiled in by ergo_embed_text (CMakeLists.txt): the page, its script and Tela's web/tela.js.
extern const char        ergo_shell_index_html[];
extern const std::size_t ergo_shell_index_html_size;
extern const char        ergo_shell_js[];
extern const std::size_t ergo_shell_js_size;
extern const char        ergo_tela_js[];
extern const std::size_t ergo_tela_js_size;

namespace ergo::studio::tela_shell {

namespace {

constexpr const char* kShellId = "shell";

net::HttpResponse file(const char* body, std::size_t size, const char* type) {
    net::HttpResponse res;
    res.status = 200;
    res.body.assign(body, size);
    res.headers["content-type"]           = type;
    res.headers["x-content-type-options"] = "nosniff";
    res.headers["cache-control"]          = "no-cache";
    res.headers["referrer-policy"]        = "no-referrer";
    return res;
}

} // namespace

ScreenAccess screen_access(const net::HttpRequest& req, const std::string& bind_host) {
    return net::is_trusted_host(req.header("host"), bind_host) ? ScreenAccess::local : ScreenAccess::published;
}

ShellRoutes::ShellRoutes(net::IHttpHandler& next_http, net::IWsAcceptor& next_ws, BrowserScreens& pages,
                         std::string bind_host)
    : next_http_(next_http), next_ws_(next_ws), pages_(pages), bind_host_(std::move(bind_host)) {}

net::HttpResponse ShellRoutes::handle(const net::HttpRequest& req) {
    if (req.path != "/shell" && req.path.rfind("/shell/", 0) != 0) return next_http_.handle(req);
    if (req.method != "GET") return net::HttpResponse::json(405, "{\"ok\":false,\"err\":\"method not allowed\"}");
    if (req.path == "/shell" || req.path == "/shell/" || req.path == "/shell/index.html") {
        auto res = file(ergo_shell_index_html, ergo_shell_index_html_size, "text/html; charset=utf-8");
        // The page draws on a canvas and overlays <video> (MediaStream from WebRTC).
        res.headers["content-security-policy"] =
            "default-src 'self'; img-src 'self' data: blob:; media-src 'self' blob:; connect-src 'self'; "
            "style-src 'self' 'unsafe-inline'; script-src 'self'; frame-src 'none'; frame-ancestors 'none'; "
            "base-uri 'none'; form-action 'none'";
        res.headers["x-frame-options"] = "DENY";
        return res;
    }
    if (req.path == "/shell/tela.js") return file(ergo_tela_js, ergo_tela_js_size, "text/javascript; charset=utf-8");
    if (req.path == "/shell/shell.js") return file(ergo_shell_js, ergo_shell_js_size, "text/javascript; charset=utf-8");
    return net::HttpResponse::json(404, "{\"ok\":false,\"err\":\"not found\"}");
}

bool ShellRoutes::accepts(const std::string& plugin_id) const {
    return plugin_id == kShellId || next_ws_.accepts(plugin_id);
}

net::WsHandlers ShellRoutes::accept(const std::string& plugin_id, const net::HttpRequest& req,
                                    std::shared_ptr<net::IWsConnection> conn) {
    if (plugin_id != kShellId) return next_ws_.accept(plugin_id, req, std::move(conn));
    return pages_.open(std::move(conn), screen_access(req, bind_host_));
}

} // namespace ergo::studio::tela_shell
