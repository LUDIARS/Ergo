/// ergo::audio::sonus::configure() / options(): the settings the Sonus backend reads
/// in Engine::initialize(). stats() lives with the engine (audio_engine_sonus.cpp).
#include "ergo/audio/sonus_options.h"

#include "ergo/audio/audio_engine.h"

#include <cstdio>
#include <mutex>

namespace ergo::audio::sonus {
namespace {

std::mutex& store_mutex() {
    static std::mutex m;
    return m;
}
Options& store() {
    static Options o;
    return o;
}

} // namespace

bool configure(const Options& o) {
    if (Engine::instance().is_initialized()) {
        std::fprintf(stderr, "[audio:sonus] configure() ignored: call it before initialize() (or after shutdown())\n");
        return false;
    }
    if (o.max_loaded_sounds < 1 || o.max_loaded_sounds > 4096) {
        std::fprintf(stderr, "[audio:sonus] configure(): max_loaded_sounds must be 1..4096 (got %u)\n",
                     static_cast<unsigned>(o.max_loaded_sounds));
        return false;
    }
    if (o.output != Output::Platform && o.output != Output::Silent && o.output != Output::Manual) {
        std::fprintf(stderr, "[audio:sonus] configure(): unknown output %u\n", static_cast<unsigned>(o.output));
        return false;
    }
#if !defined(ERGO_AUDIO_SONUS_LIVELINK)
    if (o.live_link) {
        std::fprintf(stderr, "[audio:sonus] configure(): live_link needs a Sonus built with SND_LIVELINK=ON\n");
        return false;
    }
#endif
    std::lock_guard<std::mutex> lk(store_mutex());
    store() = o;
    return true;
}

Options options() {
    std::lock_guard<std::mutex> lk(store_mutex());
    return store();
}

} // namespace ergo::audio::sonus
