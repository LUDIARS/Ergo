#pragma once
/// "musa_melpomene" plugin: Ars-Musa Melpomene (game-side issue tracking on
/// GitHub) inside the Studio. HTTP API mirrors the Tauri commands:
///   GET  /api/config               POST /api/config {config fields, github_token?}
///   GET  /api/issues?refresh=1     GET  /api/issue?number=
///   POST /api/issues {title, body?, labels?, milestone?, ticket?}
///   POST /api/issue/update {number, title?, body?, state?, labels?}
///   POST /api/issue/close {number}
///   GET  /api/comments?issue=      POST /api/comments {issue, body}
///   GET  /api/issues/by-scene?scene=     GET /api/issues/by-object?path=
///   GET  /api/issues/near?x=&y=&z=&radius=
///   GET  /api/cache-stats
///   GET  /api/notifications        (poll: open PRs with reviews + recent runs)
///   GET  /api/pulls?state=         GET  /api/reviews?pr=     GET /api/runs
///   GET  /api/milestones           POST /api/milestones {title, description?, due_on?}
///   GET  /api/user                 GET  /api/health
/// The token is write-only through the API: GET /api/config returns has_token.
#include "github_api.h"
#include "notification_tracker.h"
#include "ticket_cache.h"

#include "ergo/studio/host/plugin.h"

#include <mutex>

namespace ergo::studio::plugins::musa_melpomene {

class MelpomenePlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    MelpomenePlugin(ConfigStore store, std::shared_ptr<platform::IHttpClient> http, host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    JsonValue         snapshot() const override;

    // ---- In-process API ----
    MelpomeneConfig                config() const;
    bool                           update_config(const JsonValue& fields, std::string& err);
    ApiResult<std::vector<Ticket>> issues(bool force_refresh);
    ApiResult<NotificationState>   poll_notifications();
    GitHubApi                      api() const;   ///< built from the current config

private:
    host::PluginInfo                       info_;
    ConfigStore                            store_;
    std::shared_ptr<platform::IHttpClient> http_;
    host::LogFn                            log_;
    mutable std::mutex                     mtx_;
    MelpomeneConfig                        cfg_;
    std::string                            config_error_;
    TicketCache                            cache_;
    NotificationTracker                    tracker_;
};

std::unique_ptr<host::IPlugin> make_melpomene_plugin(const std::string& home, const host::LogFn& log);

} // namespace ergo::studio::plugins::musa_melpomene
