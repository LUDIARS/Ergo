#pragma once
/// HTTP API of the Terpsichore command server (port of Ars-Musa terpsichore
/// services/command_server.rs). Served by its own ergo_studio_net::Server on
/// the configured host:port, outside the Studio port.
///   GET  /api/health /api/status /api/compile-status /api/build-status
///        /api/game-status /api/play-status
///   POST /api/execute-command {command, args?, payload?}
///        /api/recompile  /api/build {target, output_path?, development}
///        /api/play-start /api/play-stop
/// With a token configured every POST needs "Authorization: Bearer <token>"
/// (the original checked it on execute-command only).
#include "../common/json_util.h"
#include "terpsichore_config.h"

#include "ergo/studio/net/server.h"

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>

namespace ergo::studio::plugins::musa_terpsichore {

struct CommandRequest {
    std::string              command;
    std::vector<std::string> args;
    JsonValue                payload;   ///< null when absent
};

struct CommandResult {
    bool        success = true;
    std::string message;
    JsonValue   data;                   ///< omitted from JSON when null
};
JsonValue to_json_value(const CommandResult& r);

/// Runs a command for the host (the Studio). Called on a server thread.
using CommandHandler = std::function<CommandResult(const CommandRequest&)>;

class CommandApi final : public net::IHttpHandler {
public:
    explicit CommandApi(TerpsichoreConfig cfg);

    net::HttpResponse handle(const net::HttpRequest& req) override;

    void     set_command_handler(CommandHandler h);
    /// Status JSON as GET /api/status returns it.
    JsonValue status() const;
    /// Game state the host reports (scene, custom data). Playing is kept by the API.
    void     set_game_scene(std::optional<std::string> scene, JsonValue custom_data);

private:
    bool authorized(const net::HttpRequest& req) const;

    TerpsichoreConfig                     cfg_;
    std::chrono::steady_clock::time_point started_;
    mutable std::mutex                    mtx_;
    uint64_t                              request_count_ = 0;
    bool                                  compiling_     = false;
    bool                                  building_      = false;
    std::optional<bool>                   build_success_;
    std::optional<std::string>            build_output_;
    bool                                  playing_       = false;
    std::optional<std::string>            scene_;
    JsonValue                             custom_data_;
    CommandHandler                        handler_;
};

} // namespace ergo::studio::plugins::musa_terpsichore
