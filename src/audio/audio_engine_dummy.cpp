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

namespace {

/// Dummy handles carry only a label; PCM sounds get a synthetic one so
/// play() can report "what would have played" the same way as files.
std::string pcm_label(size_t sampleCount, int sampleRate) {
    return "<pcm " + std::to_string(sampleCount) + " samples @ " +
           std::to_string(sampleRate) + "Hz>";
}

} // namespace

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

SoundHandle Engine::load_sound_pcm(const float* samples, size_t sampleCount, int sampleRate) {
    if (!impl_->initialized) return INVALID_SOUND;
    if (!samples || sampleCount == 0 || sampleRate <= 0) return INVALID_SOUND;
    const SoundHandle h = impl_->next_handle++;
    impl_->paths[h] = pcm_label(sampleCount, sampleRate);
    std::fprintf(stderr, "[audio:dummy] load_sound_pcm samples=%zu rate=%d\n",
                 sampleCount, sampleRate);
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
