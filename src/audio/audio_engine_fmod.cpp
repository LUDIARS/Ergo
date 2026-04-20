/// FMOD Core backend for ergo::audio.
///
/// Compiled when CMake can find the FMOD SDK (via `FMOD_SDK_DIR` env /
/// cache var, or the `FindFMOD.cmake` in `cmake/`). When it isn't
/// found, `audio_engine_dummy.cpp` is linked instead — so this file
/// and the dummy are mutually exclusive TUs.
///
/// Only one-shot fire-and-forget playback is wired. 3D, DSP chains, and
/// FMOD Studio events are deferred (see spec/module/audio.md roadmap).

#include "ergo/audio/audio_engine.h"

#include <cstdio>
#include <mutex>
#include <unordered_map>

#include <fmod.hpp>
#include <fmod_errors.h>

namespace ergo::audio {

namespace {

void log_fmod_err(const char* where, FMOD_RESULT r) {
    std::fprintf(stderr, "[audio:fmod] %s failed: %s\n",
                 where, FMOD_ErrorString(r));
}

} // namespace

struct Engine::Impl {
    bool                                       initialized = false;
    FMOD::System*                              sys         = nullptr;
    std::mutex                                 mtx;                 // guards the sound table
    uint32_t                                   next_handle = 1;
    std::unordered_map<SoundHandle, FMOD::Sound*> sounds;
};

Engine& Engine::instance() {
    static Engine e;
    return e;
}

Engine::Engine()  : impl_(new Impl()) {}
Engine::~Engine() { shutdown(); delete impl_; }

bool Engine::initialize() {
    if (impl_->initialized) return true;

    FMOD_RESULT r = FMOD::System_Create(&impl_->sys);
    if (r != FMOD_OK) { log_fmod_err("System_Create", r); return false; }

    // 128 virtual channels — generous enough for SE + BGM without being
    // wasteful. FMOD_INIT_NORMAL covers our one-shot scenario; when 3D
    // is added, bring in FMOD_INIT_3D_RIGHTHANDED too.
    r = impl_->sys->init(128, FMOD_INIT_NORMAL, nullptr);
    if (r != FMOD_OK) {
        log_fmod_err("System::init", r);
        impl_->sys->release();
        impl_->sys = nullptr;
        return false;
    }

    std::fprintf(stderr, "[audio] backend=FMOD\n");
    impl_->initialized = true;
    return true;
}

void Engine::shutdown() {
    if (!impl_->initialized) return;

    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        for (auto& [h, s] : impl_->sounds) {
            if (s) s->release();
        }
        impl_->sounds.clear();
    }

    if (impl_->sys) {
        impl_->sys->close();
        impl_->sys->release();
        impl_->sys = nullptr;
    }
    impl_->initialized = false;
}

void Engine::update() {
    if (!impl_->initialized || !impl_->sys) return;
    impl_->sys->update();
}

SoundHandle Engine::load_sound(const std::string& path) {
    if (!impl_->initialized || !impl_->sys) return INVALID_SOUND;

    FMOD::Sound* snd = nullptr;
    const FMOD_RESULT r =
        impl_->sys->createSound(path.c_str(),
                                FMOD_DEFAULT | FMOD_2D | FMOD_LOOP_OFF,
                                nullptr, &snd);
    if (r != FMOD_OK || !snd) {
        std::fprintf(stderr, "[audio:fmod] createSound('%s') failed: %s\n",
                     path.c_str(), FMOD_ErrorString(r));
        return INVALID_SOUND;
    }

    std::lock_guard<std::mutex> lk(impl_->mtx);
    const SoundHandle h = impl_->next_handle++;
    impl_->sounds[h] = snd;
    return h;
}

void Engine::unload_sound(SoundHandle h) {
    if (h == INVALID_SOUND) return;
    FMOD::Sound* snd = nullptr;
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        auto it = impl_->sounds.find(h);
        if (it == impl_->sounds.end()) return;
        snd = it->second;
        impl_->sounds.erase(it);
    }
    if (snd) snd->release();
}

void Engine::play(SoundHandle h, float volume, float pitch) {
    if (!impl_->initialized || !impl_->sys || h == INVALID_SOUND) return;

    FMOD::Sound* snd = nullptr;
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        auto it = impl_->sounds.find(h);
        if (it == impl_->sounds.end()) return;
        snd = it->second;
    }
    if (!snd) return;

    FMOD::Channel* ch = nullptr;
    FMOD_RESULT r = impl_->sys->playSound(snd, nullptr, /*paused=*/true, &ch);
    if (r != FMOD_OK || !ch) { log_fmod_err("playSound", r); return; }

    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    if (pitch  < 0.01f) pitch = 0.01f;

    ch->setVolume(volume);

    // setFrequency wants absolute Hz; adjust against the sound's
    // default to get a pitch ratio semantic.
    float default_freq = 0.0f;
    if (snd->getDefaults(&default_freq, nullptr) == FMOD_OK && default_freq > 0.0f) {
        ch->setFrequency(default_freq * pitch);
    }
    ch->setPaused(false);
}

Backend     Engine::backend()        const { return Backend::FMOD; }
const char* Engine::backend_name()   const { return "FMOD"; }
bool        Engine::is_initialized() const { return impl_->initialized; }

} // namespace ergo::audio
