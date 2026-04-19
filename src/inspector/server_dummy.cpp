#include "ergo/inspector/inspector.h"

/// No-op server implementation. Used when ERGO_INSPECTOR_BUILD_SERVER=OFF
/// (Phase A registry-only build) or as a placeholder before the real
/// WebSocket server is wired in.

namespace ergo::inspector {

struct Inspector::ServerState {};

bool Inspector::start_server(uint16_t /*port*/) {
    return false;
}

void Inspector::stop_server() {
    // nothing to stop
}

bool Inspector::server_running() const {
    return false;
}

} // namespace ergo::inspector
