#include "sonus_shown.h"

#include <cmath>

namespace ergo::studio::plugins::sonus {

const char* link_state_name(LinkState s) {
    switch (s) {
    case LinkState::Offline: return "offline";
    case LinkState::Connecting: return "connecting";
    case LinkState::Handshake: return "handshake";
    case LinkState::Online: return "online";
    case LinkState::Rejected: return "rejected";
    }
    return "offline";
}

float to_db(float linear) {
    if (!(linear > 0.0f)) return kFloorDb;
    const float db = 20.0f * std::log10(linear);
    return db < kFloorDb ? kFloorDb : db;
}

} // namespace ergo::studio::plugins::sonus
