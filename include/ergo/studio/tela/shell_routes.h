#pragma once
/// The Studio's own browser shell, in front of the plugin host:
///   GET /shell/            the page (canvas + web/tela.js + shell.js)
///   GET /shell/tela.js     Tela's web renderer (embedded at build time)
///   GET /shell/shell.js    the page script (socket, remote-play video)
///   WS  /shell/ws          one BrowserScreens page
/// Everything else goes to the next handler. request_guard has already run
/// (the server checks Host / Origin / the Access header before any handler);
/// a page reached under a public name gets ScreenAccess::published.
#include "ergo/studio/net/server.h"
#include "ergo/studio/tela/browser_screens.h"

#include <string>

namespace ergo::studio::tela_shell {

/// local for a loopback Host (or the bind host), published otherwise.
ScreenAccess screen_access(const net::HttpRequest& req, const std::string& bind_host);

class ShellRoutes final : public net::IHttpHandler, public net::IWsAcceptor {
public:
    ShellRoutes(net::IHttpHandler& next_http, net::IWsAcceptor& next_ws, BrowserScreens& pages, std::string bind_host);

    net::HttpResponse handle(const net::HttpRequest& req) override;
    bool              accepts(const std::string& plugin_id) const override;
    net::WsHandlers   accept(const std::string& plugin_id, const net::HttpRequest& req,
                             std::shared_ptr<net::IWsConnection> conn) override;

private:
    net::IHttpHandler& next_http_;
    net::IWsAcceptor&  next_ws_;
    BrowserScreens&    pages_;
    std::string        bind_host_;
};

} // namespace ergo::studio::tela_shell
