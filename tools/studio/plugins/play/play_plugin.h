#pragma once
/// "play" plugin: builds, runs and stops the games of the workspace profiles'
/// "play" lists, streams their window over WebRTC and takes the viewer's
/// keyboard and pointer (spec/feature/tool/play.md).
///   GET  /api/health
///   GET  /api/targets
///   POST /api/targets/<name>/{build,run,stop}
///   GET  /api/targets/<name>/logs?since=<n>
///   GET  /api/ice
///   WS   /ws            offer -> answer, key / pointer / wheel, state
#include "play_target.h"
#include "remote_play.h"
#include "target_runner.h"
#include "../common/json_util.h"
#include "../common/profile_source.h"
#include "ergo/studio/host/plugin.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ergo::studio::plugins::play {

/// Where the targets come from (the open project's profile).
using TargetSource = std::function<std::vector<PlayTarget>(std::vector<std::string>& errors)>;

struct PlayDeps {
    TargetSource                 targets;
    std::unique_ptr<IRemotePlay> remote;   ///< make_remote_play(): Odeum's, or one that refuses
    TargetRunner::StartFn        start;    ///< platform::start_live_process
};

class PlayPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginSocket,
                         public host::IPluginHealth {
public:
    PlayPlugin(PlayDeps deps, host::LogFn log);
    ~PlayPlugin() override;

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginSocket*    socket() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    net::WsHandlers   on_open(const net::HttpRequest& req, std::shared_ptr<net::IWsConnection> conn) override;
    JsonValue         snapshot() const override;

private:
    struct Client {
        std::shared_ptr<net::IWsConnection> conn;
        std::string                         viewer;   ///< "" until an offer is answered
        std::string                         target;
        std::string                         window;
    };

    std::optional<PlayTarget> find_target(const std::string& name) const;
    net::HttpResponse         list_targets() const;
    net::HttpResponse         target_action(const net::HttpRequest& req, const std::string& rest);
    void                      on_text(uint64_t client, const std::string& text);
    void                      on_offer(uint64_t client, const std::string& target, const std::string& sdp);
    void                      close_client(uint64_t client);
    void                      send(uint64_t client, const JsonValue& msg);
    void                      broadcast(const JsonValue& msg);
    JsonValue                 target_json(const PlayTarget& t) const;

    host::PluginInfo                         info_;
    host::LogFn                              log_;
    TargetSource                             targets_;
    std::unique_ptr<IRemotePlay>             remote_;
    mutable std::mutex                       clients_mutex_;
    std::map<uint64_t, Client>               clients_;
    std::unique_ptr<TargetRunner>            runner_;   // last: stopped first, its callbacks reach the rest
};

/// The plugin with the system pieces: targets from `profiles`, child
/// processes, and Odeum's remote play when linked.
/// Reads ERGO_STUDIO_FFMPEG, ERGO_STUDIO_STUN_URLS, ERGO_STUDIO_TURN_KEY_ID and
/// ERGO_STUDIO_TURN_KEY_API_TOKEN; throws when only one TURN variable is set.
std::unique_ptr<host::IPlugin> make_play_plugin(std::shared_ptr<const IProfileSource> profiles, const host::LogFn& log);

/// The targets of every profile `profiles` gives (a bad "play" or an unreadable profile is an error line).
TargetSource profile_targets(std::shared_ptr<const IProfileSource> profiles);

} // namespace ergo::studio::plugins::play
