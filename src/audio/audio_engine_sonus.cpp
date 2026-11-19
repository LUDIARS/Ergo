/// LUDIARS Sonus backend for ergo::audio (ERGO_AUDIO_BACKEND=sonus).
///
/// Only Sonus' C API (Sonus::sonus, snd/sonus.h) is used, so this file and
/// ergo_audio stay C++17; no Sonus type leaves this backend's sources. The
/// FMOD backend is the reference for the Engine contract (same arguments,
/// same return values, same failure modes) — see spec/module/audio_sonus.md
/// for what Sonus cannot do and how that is reported.
///
///   load_sound  WAV only (wav_pcm.h), decoded into memory. Anything else:
///               INVALID_SOUND + one log line (FMOD would decode OGG / MP3 ...).
///   play        queued; update() turns each play into its own static user
///               buffer (play_buffers.h), volume baked in, pitch as source rate.
///   update      owner thread (the one that called initialize()): starts the
///               queued plays, snd_update(), frees what Sonus released.
///
/// Threads: initialize / update / shutdown on the owner (main) thread, as the
/// header says for update(); load_sound / unload_sound / play from any thread.

#include "ergo/audio/audio_engine.h"
#include "ergo/audio/sonus_options.h"

#include "sonus/play_buffers.h"
#include "sonus/result_text.h"
#include "sonus/silent_output_device.h"
#include "wav_pcm.h"

#include "snd/sonus.h"

#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ergo::audio {

namespace {

using sonus_detail::PlayBuffers;
using sonus_detail::result_name;
using sonus_detail::SilentOutputDevice;

struct PendingPlay {
    std::shared_ptr<const detail::PcmSound> pcm;
    float                                   volume = 1.0f;
    float                                   pitch  = 1.0f;
};

/// What stats() / reset_output_peak() reach from any thread. Written by the owner
/// thread in initialize() / update() / shutdown(). Never destroyed: Engine's own
/// destructor (static destruction) still publishes through it.
struct SharedStats {
    std::mutex          mtx;
    sonus::Stats        stats;
    SilentOutputDevice* silent = nullptr;   // the engine's silent device while initialized
};
SharedStats& shared_stats() {
    static SharedStats* s = new SharedStats();   // NOLINT(cppcoreguidelines-owning-memory): intentionally leaked
    return *s;
}

} // namespace

struct Engine::Impl {
    std::mutex mtx;   // guards everything below except where noted

    bool            initialized = false;
    snd_system*     sys         = nullptr;   // owner thread only (Sonus' rule)
    std::thread::id owner;
    bool            wrong_thread_logged = false;

    std::unique_ptr<SilentOutputDevice> silent;
    std::unique_ptr<PlayBuffers>        plays;   // owner thread only
    uint16_t                            live_port = 0;
    uint32_t                            dropped   = 0;

    uint32_t                                                              next_handle = 1;
    std::unordered_map<SoundHandle, std::shared_ptr<const detail::PcmSound>> sounds;
    std::vector<PendingPlay>                                             pending;

    void publish_stats();
};

namespace {

void SND_CALL on_end(void* user, snd_playback_handle playback, snd_end_reason reason, snd_end_detail detail) {
    auto* plays = static_cast<PlayBuffers*>(user);
    if (!plays->ended(playback)) return;
    if (reason != SND_END_COMPLETED && reason != SND_END_STOPPED) {
        std::fprintf(stderr, "[audio:sonus] a play ended early: %s (detail %u)\n",
                     sonus_detail::end_reason_name(reason), static_cast<unsigned>(detail));
    }
}

void SND_CALL on_error(void*, snd_result code, uint32_t detail, uint64_t object) {
    std::fprintf(stderr, "[audio:sonus] error %s (detail %u, object %llu)\n", result_name(code),
                 static_cast<unsigned>(detail), static_cast<unsigned long long>(object));
}

void SND_CALL on_log(void*, snd_log_level level, const char* format, uint64_t a0, uint64_t a1) {
    if (level < SND_LOG_WARN || format == nullptr) return;
    // `format` is a static printf-style string over raw 64-bit values (snd/types.h).
    char line[512];
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    std::snprintf(line, sizeof line, format, a0, a1);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    std::fprintf(stderr, "[audio:sonus] %s: %s\n", level >= SND_LOG_ERROR ? "error" : "warn", line);
}

} // namespace

Engine& Engine::instance() {
    static Engine e;
    return e;
}

Engine::Engine() : impl_(new Impl()) {}
Engine::~Engine() {
    shutdown();
    delete impl_;
}

bool Engine::initialize() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (impl_->initialized) return true;

    const sonus::Options opt = sonus::options();
    snd_config cfg;
    snd_config_init(&cfg);
    if (opt.output == sonus::Output::Silent) {
        impl_->silent      = std::make_unique<SilentOutputDevice>();
        cfg.output.device = impl_->silent->table();
    }
    cfg.max_user_buffers = opt.max_concurrent_plays;
    cfg.max_pitch_ratio  = SND_MAX_PITCH_RATIO;   // the widest pitch range Sonus resamples
    cfg.livelink.enabled = opt.live_link ? 1u : 0u;
    if (opt.live_link_port != 0) cfg.livelink.port = opt.live_link_port;

    const snd_result r = snd_create(&cfg, nullptr, nullptr, &impl_->sys);
    if (r != SND_OK) {
        std::fprintf(stderr, "[audio:sonus] snd_create failed: %s%s\n", result_name(r),
                     r == SND_ERR_NO_DEVICE ? " (no output device; sonus::Output::Silent runs without one)" : "");
        impl_->sys = nullptr;
        impl_->silent.reset();
        return false;
    }
    impl_->plays = std::make_unique<PlayBuffers>(impl_->sys, cfg.sample_rate, cfg.max_pitch_ratio);
    snd_callbacks cb{};
    cb.user     = impl_->plays.get();
    cb.on_end   = &on_end;
    cb.on_error = &on_error;
    cb.on_log   = &on_log;
    snd_set_callbacks(impl_->sys, &cb);

    impl_->owner               = std::this_thread::get_id();
    impl_->wrong_thread_logged = false;
    impl_->live_port           = opt.live_link ? static_cast<uint16_t>(cfg.livelink.port) : 0;
    impl_->dropped             = 0;
    impl_->initialized         = true;
    impl_->publish_stats();
    std::fprintf(stderr, "[audio] backend=Sonus (%s output%s)\n",
                 opt.output == sonus::Output::Silent ? "silent" : "platform",
                 opt.live_link ? ", live link on" : "");
    if (opt.live_link) std::fprintf(stderr, "[audio:sonus] live link on port %u\n", static_cast<unsigned>(impl_->live_port));
    return true;
}

void Engine::shutdown() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (!impl_->initialized) return;
    if (std::this_thread::get_id() != impl_->owner) {
        std::fprintf(stderr, "[audio:sonus] shutdown() must run on the thread that called initialize(); "
                             "the engine stays up\n");
        return;
    }
    impl_->initialized = false;
    impl_->live_port   = 0;
    impl_->publish_stats();    // detaches the silent device from stats() first
    snd_destroy(impl_->sys);   // stops audio; Sonus references no game memory after this
    impl_->sys = nullptr;
    impl_->plays->drop_all();
    impl_->plays.reset();
    impl_->silent.reset();
    impl_->sounds.clear();
    impl_->pending.clear();
}

void Engine::update() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (!impl_->initialized) return;
    if (std::this_thread::get_id() != impl_->owner) {
        if (!impl_->wrong_thread_logged) {
            std::fprintf(stderr, "[audio:sonus] update() must run on the thread that called initialize(); ignored\n");
            impl_->wrong_thread_logged = true;
        }
        return;
    }
    for (const PendingPlay& p : impl_->pending) {
        std::string why;
        if (!impl_->plays->start(p.pcm, p.volume, p.pitch, why)) {
            ++impl_->dropped;
            std::fprintf(stderr, "[audio:sonus] play dropped: %s\n", why.c_str());
        }
    }
    impl_->pending.clear();
    const snd_result r = snd_update(impl_->sys);   // on_end callbacks run in here
    if (r != SND_OK) std::fprintf(stderr, "[audio:sonus] snd_update failed: %s\n", result_name(r));
    impl_->plays->reclaim();
    impl_->publish_stats();
}

SoundHandle Engine::load_sound(const std::string& path) {
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        if (!impl_->initialized) return INVALID_SOUND;
    }
    detail::PcmResult loaded = detail::load_wav_file(path);
    if (!loaded.ok) {
        std::fprintf(stderr, "[audio:sonus] load_sound('%s') failed: %s\n", path.c_str(), loaded.error.c_str());
        return INVALID_SOUND;
    }
    auto pcm = std::make_shared<const detail::PcmSound>(std::move(loaded.pcm));
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (!impl_->initialized) return INVALID_SOUND;   // shut down while decoding
    const SoundHandle h = impl_->next_handle++;
    impl_->sounds[h]    = std::move(pcm);
    return h;
}

void Engine::unload_sound(SoundHandle h) {
    if (h == INVALID_SOUND) return;
    // Plays already queued or sounding keep their own copy of the samples.
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->sounds.erase(h);
}

void Engine::play(SoundHandle h, float volume, float pitch) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (!impl_->initialized || h == INVALID_SOUND) return;
    auto it = impl_->sounds.find(h);
    if (it == impl_->sounds.end()) return;
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    if (pitch < 0.01f) pitch = 0.01f;
    impl_->pending.push_back({it->second, volume, pitch});
}

Backend     Engine::backend() const { return Backend::Sonus; }
const char* Engine::backend_name() const { return "Sonus"; }
bool        Engine::is_initialized() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->initialized;
}

void Engine::Impl::publish_stats() {
    sonus::Stats s;
    s.running        = initialized;
    s.live_link_port = live_port;
    s.dropped_plays  = dropped;
    if (initialized) {
        s.plays_held = static_cast<uint32_t>(plays->held());
        snd_stats st{};
        if (snd_get_stats(sys, &st) == SND_OK) s.blocks_rendered = st.block_index;
        snd_meter m{};
        if (snd_get_meter(sys, SND_ID_NONE, &m) == SND_OK) {
            for (uint32_t c = 0; c < m.channels && c < SND_MAX_CHANNELS; ++c) {
                if (m.peak[c] > s.master_peak) s.master_peak = m.peak[c];
            }
        }
        if (silent) s.output_peak = silent->output_peak();
    }
    SharedStats& shared = shared_stats();
    std::lock_guard<std::mutex> lk(shared.mtx);
    shared.stats  = s;
    shared.silent = initialized ? silent.get() : nullptr;
}

namespace sonus {

Stats stats() {
    SharedStats& shared = shared_stats();
    std::lock_guard<std::mutex> lk(shared.mtx);
    Stats s = shared.stats;
    if (shared.silent) s.output_peak = shared.silent->output_peak();   // live, not as of the last update
    return s;
}

void reset_output_peak() {
    SharedStats& shared = shared_stats();
    std::lock_guard<std::mutex> lk(shared.mtx);
    if (shared.silent) shared.silent->reset_output_peak();
    shared.stats.output_peak = 0.0f;
}

} // namespace sonus

} // namespace ergo::audio
