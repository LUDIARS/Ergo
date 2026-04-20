/// Dummy audio backend. Always available; picked when FMOD SDK isn't
/// visible to CMake. Every API call is a no-op apart from hand-back of
/// monotonically increasing handles and a one-line log on play().

#include "ergo/audio/audio_engine.h"

#include <cstdio>
#include <unordered_map>

namespace ergo::audio {

struct Engine::Impl {
    bool                                     initialized = false;
    uint32_t                                 next_handle = 1;
    std::unordered_map<SoundHandle, std::string> paths;
};

Engine& Engine::instance() {
    static Engine e;
    return e;
}

Engine::Engine()  : impl_(new Impl()) {}
Engine::~Engine() { delete impl_; }

bool Engine::initialize() {
    if (impl_->initialized) return true;
    std::fprintf(stderr, "[audio] backend=Dummy (FMOD SDK not compiled in)\n");
    impl_->initialized = true;
    return true;
}

void Engine::shutdown() {
    if (!impl_->initialized) return;
    impl_->paths.clear();
    impl_->initialized = false;
}

void Engine::update() {
    // nothing
}

SoundHandle Engine::load_sound(const std::string& path) {
    if (!impl_->initialized) return INVALID_SOUND;
    const SoundHandle h = impl_->next_handle++;
    impl_->paths[h] = path;
    return h;
}

void Engine::unload_sound(SoundHandle h) {
    if (h == INVALID_SOUND) return;
    impl_->paths.erase(h);
}

void Engine::play(SoundHandle h, float volume, float pitch) {
    if (!impl_->initialized || h == INVALID_SOUND) return;
    auto it = impl_->paths.find(h);
    if (it == impl_->paths.end()) return;
    std::fprintf(stderr, "[audio:dummy] play %s vol=%.2f pitch=%.2f\n",
                 it->second.c_str(), volume, pitch);
}

Backend     Engine::backend()      const { return Backend::Dummy; }
const char* Engine::backend_name() const { return "Dummy"; }
bool        Engine::is_initialized() const { return impl_->initialized; }

} // namespace ergo::audio
