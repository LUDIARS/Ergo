#pragma once
/// The launcher's HTTP API, in front of the next handler:
///   GET  /api/launcher/projects       {open: {source, name, root, style, domain} | null, projects: [row], notice}
///   POST /api/launcher/open           {source}
///   POST /api/launcher/close          {}
///   POST /api/launcher/add            {source}   loopback only
///   POST /api/launcher/remove         {source}   loopback only
/// Adding and removing change the list on this machine, so a request that came
/// in on the remote gate (net::Gate::remote) is refused, like the other
/// loopback-only operations (ingress/remote_scope.h). The entrance check has
/// already run.
#include "ergo/studio/net/server.h"
#include "ergo/studio/project/launcher.h"

#include <string>

namespace ergo::studio::project {

class LauncherRoutes final : public net::IHttpHandler {
public:
    LauncherRoutes(Launcher& launcher, net::IHttpHandler& next);

    net::HttpResponse handle(const net::HttpRequest& req) override;

private:
    net::HttpResponse list() const;
    net::HttpResponse act(const std::string& what, const net::HttpRequest& req);

    Launcher&          launcher_;
    net::IHttpHandler& next_;
};

} // namespace ergo::studio::project
