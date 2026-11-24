#pragma once
/// The Studio's own browser shell, in front of the plugin host:
///   GET /shell/            the page (canvas + web/tela.js + shell.js)
///   GET /shell/tela.js     Tela's web renderer (embedded at build time)
///   GET /shell/shell.js    the page script (socket, remote-play video)
///   WS  /shell/ws          one BrowserScreens page
/// Everything else goes to the next handler. The entrance check has already
/// run (ingress/entrance.h); a page opened on the remote gate gets
/// ScreenAccess::published, whatever its Host.
#include "ergo/studio/net/server.h"
#include "ergo/studio/tela/browser_screens.h"

#include <string>

namespace ergo::studio::tela_shell {

/// local for a request on the local gate, published for the remote gate.
ScreenAccess screen_access(const net::HttpRequest& req);

class ShellRoutes final : public net::IHttpHandler, public net::IWsAcceptor {
public:
    ShellRoutes(net::IHttpHandler& next_http, net::IWsAcceptor& next_ws, BrowserScreens& pages);

    net::HttpResponse handle(const net::HttpRequest& req) override;
    bool              accepts(const std::string& plugin_id) const override;
    net::WsHandlers   accept(const std::string& plugin_id, const net::HttpRequest& req,
                             std::shared_ptr<net::IWsConnection> conn) override;

private:
    net::IHttpHandler& next_http_;
    net::IWsAcceptor&  next_ws_;
    BrowserScreens&    pages_;
};

} // namespace ergo::studio::tela_shell
