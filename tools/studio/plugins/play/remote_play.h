#pragma once
/// What the play plugin needs from remote play: the video (WebRTC), input
/// into the game's window and the ICE servers for the browser. Odeum owns
/// remote play (Odeum spec/feature/remote-play-library.md); this is the
/// plugin's port to it (DIP). remote_play_odeum.cpp adapts odeum_remote_play
/// (a build with ERGO_ODEUM_SOURCE_DIR); remote_play_unavailable.cpp refuses
/// everything, so building, running and logs work without it.
#include "play_target.h"
#include "../common/json_util.h"
#include "ergo/studio/host/plugin.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace ergo::studio::plugins::play {

enum class PointerPhase { Down, Up, Move };

class IRemotePlay {
public:
    virtual ~IRemotePlay() = default;
    virtual bool streaming() const = 0;   ///< WebRTC available
    virtual bool input() const     = 0;   ///< window input available
    virtual bool turn() const      = 0;   ///< a TURN key is configured

    /// `[{urls, username?, credential?}]` for the browser (STUN, and TURN when
    /// configured). `warning` says why TURN is missing, without the key.
    virtual JsonValue ice_servers(std::string* warning) = 0;

    /// Answer a browser's offer for `target`. `on_closed` runs once when the
    /// stream ends by itself. False with `err` ("webrtc_unavailable", "no_h264", ...).
    virtual bool        open(const std::string& viewer, const PlayTarget& target, const std::string& offer_sdp,
                             std::function<void()> on_closed, std::string& answer_sdp, std::string& err) = 0;
    virtual void        close(const std::string& viewer) = 0;
    virtual std::size_t viewers(const std::string& target) const = 0;

    /// True when `name` is a key the input accepts (Odeum's key-name table).
    virtual bool known_key(const std::string& name) const = 0;
    /// Input into the window titled `window`. False (with `err`) when it was not delivered.
    virtual bool key(const std::string& window, const std::string& name, bool down, std::string& err) = 0;
    virtual bool pointer(const std::string& window, double x, double y, int button, PointerPhase phase,
                         std::string& err) = 0;
    virtual bool wheel(const std::string& window, double x, double y, int notches, std::string& err) = 0;
};

struct RemotePlayConfig {
    std::string ffmpeg = "ffmpeg";
    std::optional<std::string> stun_urls;   ///< ERGO_STUDIO_STUN_URLS
    std::string turn_key_id, turn_key_token;
};

/// The build's remote play: Odeum's when linked, otherwise one that refuses.
std::unique_ptr<IRemotePlay> make_remote_play(const RemotePlayConfig& config, host::LogFn log);

} // namespace ergo::studio::plugins::play
