#pragma once
/// The launcher's HTTP API, in front of the next handler:
///   GET  /api/launcher/projects       {open: {source, name, root, style, domain} | null, projects: [row], notice}
///   POST /api/launcher/open           {source}
///   POST /api/launcher/close          {}
///   POST /api/launcher/add            {source}   loopback only
///   POST /api/launcher/remove         {source}   loopback only
/// Adding and removing change the list on this machine, so a request under a
/// public (Access) host name is refused, like the other loopback-only
/// operations (spec/feature/tool/web.md). request_guard has already run.
#include "ergo/studio/net/server.h"
#include "ergo/studio/project/launcher.h"

#include <string>

namespace ergo::studio::project {

class LauncherRoutes final : public net::IHttpHandler {
public:
    LauncherRoutes(Launcher& launcher, net::IHttpHandler& next, std::string bind_host);

    net::HttpResponse handle(const net::HttpRequest& req) override;

private:
    net::HttpResponse list() const;
    net::HttpResponse act(const std::string& what, const net::HttpRequest& req);

    Launcher&          launcher_;
    net::IHttpHandler& next_;
    std::string        bind_host_;
};

} // namespace ergo::studio::project
