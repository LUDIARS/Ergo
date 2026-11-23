#include "play_message.h"

#include "../common/json_parse.h"

#include <algorithm>
#include <cmath>

namespace ergo::studio::plugins::play {
namespace {

double unit(const JsonValue& msg, const char* key) {
    const JsonValue* v = field(msg, key);
    if (v == nullptr || !v->is_number() || !std::isfinite(v->n)) return 0.0;
    return std::clamp(v->n, 0.0, 1.0);
}

std::string text(const JsonValue& msg, const char* key) {
    const JsonValue* v = field(msg, key);
    return v != nullptr && v->is_string() ? v->s : std::string();
}

PlayMessage offer(const JsonValue& msg) {
    OfferMessage m{text(msg, "target"), text(msg, "sdp")};
    if (m.target.empty()) return BadMessage{"offer needs a target"};
    if (m.sdp.empty() || m.sdp.size() > kMaxSdp) return BadMessage{"offer needs an sdp of at most 32 KiB"};
    return m;
}

PlayMessage key(const JsonValue& msg) {
    KeyMessage m;
    m.name = text(msg, "key");
    if (!is_key_name(m.name)) return BadMessage{"unknown key"};
    const JsonValue* down = field(msg, "down");
    m.down = down != nullptr && down->as_bool();
    return m;
}

PlayMessage pointer(const JsonValue& msg) {
    PointerMessage m;
    m.x = unit(msg, "x");
    m.y = unit(msg, "y");
    const JsonValue* button = field(msg, "button");
    const double     b      = button != nullptr && button->is_number() ? button->n : 0.0;
    m.button = b == 1 ? 1 : b == 2 ? 2 : 0;
    const std::string phase = text(msg, "phase");
    if (phase == "down") m.phase = PointerPhase::Down;
    else if (phase == "up") m.phase = PointerPhase::Up;
    else if (phase == "move") m.phase = PointerPhase::Move;
    else return BadMessage{"pointer phase must be down, up or move"};
    return m;
}

PlayMessage wheel(const JsonValue& msg) {
    WheelMessage m;
    m.x = unit(msg, "x");
    m.y = unit(msg, "y");
    const JsonValue* delta = field(msg, "delta");
    const double     d     = delta != nullptr && delta->is_number() && std::isfinite(delta->n) ? delta->n : 0.0;
    m.notches = static_cast<int>(std::lround(std::clamp(d, -10.0, 10.0)));
    if (m.notches == 0) return BadMessage{"wheel delta must be a non-zero number"};
    return m;
}

} // namespace

PlayMessage parse_play_message(const std::string& raw) {
    if (raw.size() > kMaxWsMessage) return BadMessage{"message larger than 64 KiB"};
    const auto json = parse_json(raw);
    if (!json || !json->is_object()) return BadMessage{"not a JSON object"};
    const std::string type = text(*json, "type");
    if (type == "offer") return offer(*json);
    if (type == "key") return key(*json);
    if (type == "pointer") return pointer(*json);
    if (type == "wheel") return wheel(*json);
    return BadMessage{"unknown message type"};
}

} // namespace ergo::studio::plugins::play
