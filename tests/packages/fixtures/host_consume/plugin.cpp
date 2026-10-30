// Shared module of tests/packages/test_host_consume.cmake (a host plugin /
// editor module). Linking Ergo's static libraries into it needs them to be
// position-independent.
#include "ergo/bind/bind.h"
#include "ergo/log/log.h"

extern "C" int host_plugin_entry() {
    static int value = 7;
    ERGO_LOG_DEBUG("[host_plugin] loaded");
    return ergo::bind::Engine::instance().bind("host_plugin.value", &value)
        != ergo::bind::INVALID_HANDLE ? 0 : 1;
}
