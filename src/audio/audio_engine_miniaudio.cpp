/// miniaudio backend for ergo::audio.
///
/// Compiled when CMake could not find the FMOD SDK but miniaudio was
/// available (fetched from its pinned upstream — see
/// `cmake/deps/miniaudio.cmake`). This is the backend that gives the
/// dev machines and CI *actual sound* without any proprietary SDK, and
/// it is the one that makes `load_sound_pcm` useful for Figmentum's
/// runtime-synthesised waveforms.
///
/// This TU, `audio_engine_fmod.cpp` and `audio_engine_dummy.cpp` are
/// mutually exclusive — exactly one is linked into `ergo_audio`.
///
/// Ownership model (Ergo resource-lifetime rule: whoever allocates
/// frees on every path):
///   - A *sound* is a passive recipe: either a file path or a copy of
///     the caller's PCM block. It owns no device resources.
///   - A *voice* is one in-flight playback instance. It owns its
///     `ma_sound` and, for PCM, its own `ma_audio_buffer` so playback
///     cursors are never shared between overlapping one-shots. Voices
///     are reaped in `update()` once `ma_sound_at_end` reports done,
///     and unconditionally in `shutdown()`.
///   - PCM sample storage is refcounted, so a voice started before
///     `unload_sound` keeps reading valid memory until it finishes.

#include "ergo/audio/audio_engine.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// The single implementation site for miniaudio in the whole repository.
// Every other TU that ever needs the API must include the header
// *without* this define.
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace ergo::audio {

namespace {

using PcmData = std::vector<float>;

/// A loaded sound: a recipe for producing voices. Exactly one of
/// `path` / `pcm` is meaningful, selected by `is_pcm`.
struct SoundDef {
    bool                      is_pcm      = false;
    std::string               path;                 // file sounds
    std::shared_ptr<PcmData>  pcm;                   // PCM sounds (mono f32)
    int                       sample_rate = 0;       // PCM sounds
};

/// One in-flight playback instance.
struct Voice {
    ma_sound                 sound{};
    ma_audio_buffer          buffer{};
    bool                     sound_inited  = false;
    bool                     buffer_inited = false;
    std::shared_ptr<PcmData> pcm;   // keeps buffer memory alive while playing

    ~Voice() {
        if (sound_inited)  ma_sound_uninit(&sound);
        if (buffer_inited) ma_audio_buffer_uninit(&buffer);
    }
};

float clamp_volume(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float clamp_pitch(float p) {
    return (p < 0.01f) ? 0.01f : p;
}

bool file_readable(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

} // namespace

struct Engine::Impl {
    bool        initialized = false;
    ma_engine   engine{};
    std::mutex  mtx;                 // guards the sound table + voice list
    uint32_t    next_handle = 1;
    std::unordered_map<SoundHandle, SoundDef>  sounds;
    std::vector<std::unique_ptr<Voice>>        voices;
};

Engine& Engine::instance() {
    static Engine e;
    return e;
}

Engine::Engine()  : impl_(new Impl()) {}
Engine::~Engine() { shutdown(); delete impl_; }

bool Engine::initialize() {
    if (impl_->initialized) return true;

    const ma_result r = ma_engine_init(nullptr, &impl_->engine);
    if (r != MA_SUCCESS) {
        // Headless CI / no audio device: stay uninitialized so every
        // subsequent call is a safe no-op (same contract as FMOD).
        std::fprintf(stderr, "[audio:miniaudio] ma_engine_init failed (%d)\n",
                     static_cast<int>(r));
        return false;
    }

    std::fprintf(stderr, "[audio] backend=miniaudio\n");
    impl_->initialized = true;
    return true;
}

void Engine::shutdown() {
    if (!impl_->initialized) return;

    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->voices.clear();   // ~Voice uninits sound + buffer
        impl_->sounds.clear();
    }

    ma_engine_uninit(&impl_->engine);
    impl_->initialized = false;
}

void Engine::update() {
    if (!impl_->initialized) return;

    // Reap finished voices. miniaudio's engine mixes on its own thread,
    // so this is purely lifetime bookkeeping.
    std::lock_guard<std::mutex> lk(impl_->mtx);
    for (auto it = impl_->voices.begin(); it != impl_->voices.end(); ) {
        if ((*it)->sound_inited && ma_sound_at_end(&(*it)->sound)) {
            it = impl_->voices.erase(it);
        } else {
            ++it;
        }
    }
}

SoundHandle Engine::load_sound(const std::string& path) {
    if (!impl_->initialized) return INVALID_SOUND;

    // Fail at load time like FMOD's createSound does, rather than
    // handing back a handle that can never make a sound.
    if (!file_readable(path)) {
        std::fprintf(stderr, "[audio:miniaudio] load_sound('%s') failed: not readable\n",
                     path.c_str());
        return INVALID_SOUND;
    }

    SoundDef def;
    def.is_pcm = false;
    def.path   = path;

    std::lock_guard<std::mutex> lk(impl_->mtx);
    const SoundHandle h = impl_->next_handle++;
    impl_->sounds.emplace(h, std::move(def));
    return h;
}

SoundHandle Engine::load_sound_pcm(const float* samples, size_t sampleCount, int sampleRate) {
    if (!impl_->initialized) return INVALID_SOUND;
    if (!samples || sampleCount == 0 || sampleRate <= 0) return INVALID_SOUND;

    SoundDef def;
    def.is_pcm      = true;
    def.sample_rate = sampleRate;
    def.pcm         = std::make_shared<PcmData>(samples, samples + sampleCount);

    std::lock_guard<std::mutex> lk(impl_->mtx);
    const SoundHandle h = impl_->next_handle++;
    impl_->sounds.emplace(h, std::move(def));
    return h;
}

void Engine::unload_sound(SoundHandle h) {
    if (h == INVALID_SOUND || !impl_->initialized) return;
    std::lock_guard<std::mutex> lk(impl_->mtx);
    // Voices already started keep their own shared_ptr to the samples,
    // so erasing the recipe here can never pull memory out from under
    // playback; they are reaped normally by update() / shutdown().
    impl_->sounds.erase(h);
}

void Engine::play(SoundHandle h, float volume, float pitch) {
    if (!impl_->initialized || h == INVALID_SOUND) return;

    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->sounds.find(h);
    if (it == impl_->sounds.end()) return;
    const SoundDef& def = it->second;

    auto voice = std::make_unique<Voice>();

    if (def.is_pcm) {
        if (!def.pcm || def.pcm->empty()) return;
        voice->pcm = def.pcm;

        // A fresh ma_audio_buffer per voice: overlapping one-shots of
        // the same sound must not share a read cursor. `_init` (not
        // `_init_copy`) references our memory — kept alive by voice->pcm.
        ma_audio_buffer_config bufCfg = ma_audio_buffer_config_init(
            ma_format_f32,
            /*channels=*/1,
            static_cast<ma_uint64>(voice->pcm->size()),
            voice->pcm->data(),
            /*pAllocationCallbacks=*/nullptr);
        bufCfg.sampleRate = static_cast<ma_uint32>(def.sample_rate);

        if (ma_audio_buffer_init(&bufCfg, &voice->buffer) != MA_SUCCESS) {
            std::fprintf(stderr, "[audio:miniaudio] ma_audio_buffer_init failed\n");
            return;
        }
        voice->buffer_inited = true;

        if (ma_sound_init_from_data_source(&impl_->engine, &voice->buffer,
                                           /*flags=*/0, /*pGroup=*/nullptr,
                                           &voice->sound) != MA_SUCCESS) {
            std::fprintf(stderr, "[audio:miniaudio] ma_sound_init_from_data_source failed\n");
            return;  // ~Voice uninits the buffer
        }
        voice->sound_inited = true;
    } else {
        if (ma_sound_init_from_file(&impl_->engine, def.path.c_str(),
                                    /*flags=*/MA_SOUND_FLAG_DECODE,
                                    /*pGroup=*/nullptr, /*pDoneFence=*/nullptr,
                                    &voice->sound) != MA_SUCCESS) {
            std::fprintf(stderr, "[audio:miniaudio] ma_sound_init_from_file('%s') failed\n",
                         def.path.c_str());
            return;
        }
        voice->sound_inited = true;
    }

    ma_sound_set_looping(&voice->sound, MA_FALSE);
    ma_sound_set_volume(&voice->sound, clamp_volume(volume));
    ma_sound_set_pitch(&voice->sound, clamp_pitch(pitch));

    if (ma_sound_start(&voice->sound) != MA_SUCCESS) {
        std::fprintf(stderr, "[audio:miniaudio] ma_sound_start failed\n");
        return;  // ~Voice cleans up
    }

    impl_->voices.push_back(std::move(voice));
}

Backend     Engine::backend()        const { return Backend::MiniAudio; }
const char* Engine::backend_name()   const { return "miniaudio"; }
bool        Engine::is_initialized() const { return impl_->initialized; }

} // namespace ergo::audio
