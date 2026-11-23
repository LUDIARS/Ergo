#pragma once
/// Messages a browser sends on WS /play/ws (spec/feature/tool/play.md).
/// Values are clamped or rejected here, so the rest only sees valid input
/// (PLAY-INV-02, PLAY-INV-04).
#include "remote_play.h"

#include <cstddef>
#include <string>
#include <variant>

namespace ergo::studio::plugins::play {

constexpr std::size_t kMaxWsMessage = 64 * 1024;
constexpr std::size_t kMaxSdp       = 32 * 1024;   // as Odeum's odeum::remote_play::kMaxSdp

struct OfferMessage {
    std::string target;
    std::string sdp;
};
struct KeyMessage {
    std::string name;   ///< checked against the input's key table by the plugin
    bool        down = false;
};
struct PointerMessage {
    double                 x = 0, y = 0;
    int                    button = 0;
    PointerPhase phase  = PointerPhase::Move;
};
struct WheelMessage {
    double x = 0, y = 0;
    int    notches = 0;
};
struct BadMessage {
    std::string error;   ///< shown to the browser as {type:"error"}
};

using PlayMessage = std::variant<OfferMessage, KeyMessage, PointerMessage, WheelMessage, BadMessage>;

PlayMessage parse_play_message(const std::string& text);

} // namespace ergo::studio::plugins::play
