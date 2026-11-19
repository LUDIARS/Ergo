/// LUDIARS Sonus backend for ergo::audio (ERGO_AUDIO_BACKEND=sonus).
///
/// Only Sonus' C API (Sonus::sonus, snd/sonus.h) is used, so this file and
/// ergo_audio stay C++17; no Sonus type leaves this backend's sources. The
/// FMOD backend is the reference for the Engine contract (same arguments,
/// same return values, same failure modes) — see spec/module/audio_sonus.md
/// for what Sonus cannot do and how that is reported.
///
///   load_sound  WAV only (wav_pcm.h), decoded into memory and handed to Sonus
///               as one static user buffer (sound_buffers.h). Anything else:
///               INVALID_SOUND + one log line (FMOD would decode OGG / MP3 ...).
///   play        queued; update() starts it with its own volume and pitch
///               (snd_play_user_buffer_ex), without copying the samples.
///   update      owner thread (the one that called initialize()): makes the
///               buffers of sounds loaded on other threads, releases unloaded
///               ones, starts the queued plays, snd_update(), frees what Sonus released.
///
/// Threads: initialize / update / shutdown on the owner (main) thread, as the
/// header says for update(); load_sound / unload_sound / play from any thread.

#include "ergo/audio/audio_engine.h"
#include "ergo/audio/sonus_options.h"

#include "sonus/result_text.h"
#include "sonus/sound_buffers.h"
#include "wav_pcm.h"

#include "snd/sonus.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ergo::audio {

namespace {

using sonus_detail::result_name;
using sonus_detail::SoundBuffers;

struct PendingPlay {
    SoundHandle sound  = INVALID_SOUND;
    float       volume = 1.0f;
    float       pitch  = 1.0f;
};

/// What stats() / reset_output_peak() / advance_output() reach from any thread.
/// Written by the owner thread in initialize() / update() / shutdown(). Never
/// destroyed: Engine's own destructor (static destruction) still publishes through it.
struct Shared {
    std::mutex      mtx;
    sonus::Stats    stats;
    snd_system*     manual_sys = nullptr;   // the system while it runs Output::Manual
    std::thread::id owner;
    uint32_t        block_size = 0, channels = 0;
    float           output_peak = 0.0f;
    bool            advance_refusal_logged = false;
};
Shared& shared() {
    static Shared* s = new Shared();   // NOLINT(cppcoreguidelines-owning-memory): intentionally leaked
    return *s;
}

snd_output_kind output_kind(sonus::Output o) {
    switch (o) {
    case sonus::Output::Silent: return SND_OUTPUT_SILENT;
    case sonus::Output::Manual: return SND_OUTPUT_MANUAL;
    case sonus::Output::Platform: break;
    }
    return SND_OUTPUT_PLATFORM;
}

const char* output_name(sonus::Output o) {
    switch (o) {
    case sonus::Output::Silent: return "silent";
    case sonus::Output::Manual: return "manual";
    case sonus::Output::Platform: break;
    }
    return "platform";
}

} // namespace

struct Engine::Impl {
    std::mutex mtx;   // guards everything below

    bool            initialized = false;
    snd_system*     sys         = nullptr;   // owner thread only (Sonus' rule)
    std::thread::id owner;
    bool            wrong_thread_logged = false;
    sonus::Output   output    = sonus::Output::Platform;
    uint16_t        live_port = 0;
    uint32_t        dropped   = 0;

    std::unique_ptr<SoundBuffers> buffers;   // owner thread only
    uint32_t                      next_handle = 1;
    std::unordered_set<SoundHandle> loaded;  // handles load_sound() returned and unload_sound() did not take back
    /// Decoded on another thread: the buffer is made by the next update().
    std::unordered_map<SoundHandle, std::shared_ptr<const detail::PcmSound>> to_create;
    std::vector<SoundHandle> to_release;
    std::vector<PendingPlay> pending;

    bool on_owner() const { return std::this_thread::get_id() == owner; }
    /// The buffer of `h` (owner thread). false: logged, `h` is no longer loaded.
    bool create(SoundHandle h, std::shared_ptr<const detail::PcmSound> pcm);
    void publish_stats();
};

namespace {

void SND_CALL on_end(void* user, snd_playback_handle playback, snd_end_reason reason, snd_end_detail detail) {
    static_cast<SoundBuffers*>(user)->ended(playback);
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

bool Engine::Impl::create(SoundHandle h, std::shared_ptr<const detail::PcmSound> pcm) {
    std::string why;
    if (buffers->create(h, std::move(pcm), why)) return true;
    std::fprintf(stderr, "[audio:sonus] sound %u not loaded: %s\n", static_cast<unsigned>(h), why.c_str());
    loaded.erase(h);
    return false;
}

bool Engine::initialize() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (impl_->initialized) return true;

    const sonus::Options opt = sonus::options();
    snd_config cfg;
    snd_config_init(&cfg);
    cfg.output_kind      = output_kind(opt.output);   // Sonus' own devices (D-102); output.device stays NULL
    cfg.max_user_buffers = opt.max_loaded_sounds;
    cfg.max_pitch_ratio  = SND_MAX_PITCH_RATIO;       // the widest pitch range Sonus resamples
    cfg.livelink.enabled = opt.live_link ? 1u : 0u;
    if (opt.live_link_port != 0) cfg.livelink.port = opt.live_link_port;

    const snd_result r = snd_create(&cfg, nullptr, nullptr, &impl_->sys);
    if (r != SND_OK) {
        std::fprintf(stderr, "[audio:sonus] snd_create failed: %s%s\n", result_name(r),
                     r == SND_ERR_NO_DEVICE ? " (no output device; sonus::Output::Silent runs without one)" : "");
        impl_->sys = nullptr;
        return false;
    }
    impl_->buffers = std::make_unique<SoundBuffers>(impl_->sys, cfg.sample_rate, cfg.max_pitch_ratio);
    snd_callbacks cb{};
    cb.user     = impl_->buffers.get();
    cb.on_end   = &on_end;
    cb.on_error = &on_error;
    cb.on_log   = &on_log;
    snd_set_callbacks(impl_->sys, &cb);

    impl_->owner               = std::this_thread::get_id();
    impl_->wrong_thread_logged = false;
    impl_->output              = opt.output;
    impl_->live_port           = opt.live_link ? static_cast<uint16_t>(cfg.livelink.port) : 0;
    impl_->dropped             = 0;
    impl_->initialized         = true;
    {
        Shared& sh = shared();
        std::lock_guard<std::mutex> slk(sh.mtx);
        sh.manual_sys             = opt.output == sonus::Output::Manual ? impl_->sys : nullptr;
        sh.owner                  = impl_->owner;
        sh.block_size             = cfg.block_size;
        sh.channels               = cfg.output_channels != 0 ? cfg.output_channels : 2u;
        sh.output_peak            = 0.0f;
        sh.advance_refusal_logged = false;
    }
    impl_->publish_stats();
    std::fprintf(stderr, "[audio] backend=Sonus (%s output%s)\n", output_name(opt.output),
                 opt.live_link ? ", live link on" : "");
    if (opt.live_link) std::fprintf(stderr, "[audio:sonus] live link on port %u\n", static_cast<unsigned>(impl_->live_port));
    return true;
}

void Engine::shutdown() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (!impl_->initialized) return;
    if (!impl_->on_owner()) {
        std::fprintf(stderr, "[audio:sonus] shutdown() must run on the thread that called initialize(); "
                             "the engine stays up\n");
        return;
    }
    impl_->initialized = false;
    impl_->live_port   = 0;
    {
        Shared& sh = shared();
        std::lock_guard<std::mutex> slk(sh.mtx);
        sh.manual_sys = nullptr;   // advance_output() no longer reaches the system
    }
    snd_destroy(impl_->sys);   // stops audio; Sonus references no game memory after this
    impl_->sys = nullptr;
    impl_->buffers->drop_all();
    impl_->buffers.reset();
    impl_->loaded.clear();
    impl_->to_create.clear();
    impl_->to_release.clear();
    impl_->pending.clear();
    impl_->publish_stats();
}

void Engine::update() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (!impl_->initialized) return;
    if (!impl_->on_owner()) {
        if (!impl_->wrong_thread_logged) {
            std::fprintf(stderr, "[audio:sonus] update() must run on the thread that called initialize(); ignored\n");
            impl_->wrong_thread_logged = true;
        }
        return;
    }
    for (auto& [h, pcm] : impl_->to_create) impl_->create(h, std::move(pcm));
    impl_->to_create.clear();
    for (const SoundHandle h : impl_->to_release) impl_->buffers->release(h);
    impl_->to_release.clear();
    for (const PendingPlay& p : impl_->pending) {
        std::string why;
        if (!impl_->buffers->play(p.sound, p.volume, p.pitch, why)) {
            ++impl_->dropped;
            std::fprintf(stderr, "[audio:sonus] play dropped: %s\n", why.c_str());
        }
    }
    impl_->pending.clear();
    const snd_result r = snd_update(impl_->sys);   // on_end callbacks run in here
    if (r != SND_OK) std::fprintf(stderr, "[audio:sonus] snd_update failed: %s\n", result_name(r));
    impl_->buffers->reclaim();
    impl_->publish_stats();
}

SoundHandle Engine::load_sound(const std::string& path) {
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        if (!impl_->initialized) return INVALID_SOUND;
    }
    detail::PcmResult decoded = detail::load_wav_file(path);
    if (!decoded.ok) {
        std::fprintf(stderr, "[audio:sonus] load_sound('%s') failed: %s\n", path.c_str(), decoded.error.c_str());
        return INVALID_SOUND;
    }
    auto pcm = std::make_shared<const detail::PcmSound>(std::move(decoded.pcm));
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (!impl_->initialized) return INVALID_SOUND;   // shut down while decoding
    const SoundHandle h = impl_->next_handle++;
    impl_->loaded.insert(h);
    if (!impl_->on_owner()) {
        // Sonus records only from registered threads: the owner makes the buffer in update().
        impl_->to_create[h] = std::move(pcm);
        return h;
    }
    if (!impl_->create(h, std::move(pcm))) {
        std::fprintf(stderr, "[audio:sonus] load_sound('%s') failed (see above)\n", path.c_str());
        return INVALID_SOUND;
    }
    return h;
}

void Engine::unload_sound(SoundHandle h) {
    if (h == INVALID_SOUND) return;
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (!impl_->initialized || impl_->loaded.erase(h) == 0) return;
    if (impl_->to_create.erase(h) != 0) return;   // never reached Sonus
    // Plays of the sound stop (Sonus destroys the buffer under them, as FMOD's Sound::release does).
    if (impl_->on_owner()) {
        impl_->buffers->release(h);
    } else {
        impl_->to_release.push_back(h);
    }
}

void Engine::play(SoundHandle h, float volume, float pitch) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (!impl_->initialized || h == INVALID_SOUND || impl_->loaded.count(h) == 0) return;
    if (!(volume >= 0.0f)) volume = 0.0f;   // also NaN
    if (volume > 1.0f) volume = 1.0f;
    if (!(pitch >= 0.01f)) pitch = 0.01f;
    if (std::isinf(pitch)) pitch = 16.0f;
    impl_->pending.push_back({h, volume, pitch});
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
        s.plays_active = static_cast<uint32_t>(buffers->active_plays());
        s.sounds_held  = static_cast<uint32_t>(buffers->held());
        snd_stats st{};
        if (snd_get_stats(sys, &st) == SND_OK) s.blocks_rendered = st.block_index;
        snd_meter m{};
        if (snd_get_meter(sys, SND_ID_NONE, &m) == SND_OK) {
            for (uint32_t c = 0; c < m.channels && c < SND_MAX_CHANNELS; ++c) {
                if (m.peak[c] > s.master_peak) s.master_peak = m.peak[c];
            }
        }
    }
    Shared& sh = shared();
    std::lock_guard<std::mutex> lk(sh.mtx);
    sh.stats = s;
}

namespace sonus {

Stats stats() {
    Shared& sh = shared();
    std::lock_guard<std::mutex> lk(sh.mtx);
    Stats s       = sh.stats;
    s.output_peak = sh.output_peak;
    return s;
}

void reset_output_peak() {
    Shared& sh = shared();
    std::lock_guard<std::mutex> lk(sh.mtx);
    sh.output_peak = 0.0f;
}

bool advance_output(uint32_t periods) {
    Shared&     sh = shared();
    snd_system* sys;
    uint32_t    frames, channels;
    {
        std::lock_guard<std::mutex> lk(sh.mtx);
        const char* why = sh.manual_sys == nullptr               ? "the engine is not initialized with Output::Manual"
                          : std::this_thread::get_id() != sh.owner ? "call it on the thread that called initialize()"
                                                                   : nullptr;
        if (why) {
            if (!sh.advance_refusal_logged) std::fprintf(stderr, "[audio:sonus] advance_output() refused: %s\n", why);
            sh.advance_refusal_logged = true;
            return false;
        }
        sys      = sh.manual_sys;
        frames   = periods * sh.block_size;
        channels = sh.channels;
    }
    // Only the owner thread gets here, and shutdown() runs on it too: `sys` stays valid.
    std::vector<float> out(static_cast<std::size_t>(frames) * channels);
    const snd_result   r = snd_advance_output(sys, periods, out.data(), frames);
    if (r != SND_OK) {
        std::fprintf(stderr, "[audio:sonus] snd_advance_output failed: %s\n", result_name(r));
        return false;
    }
    float m = 0.0f;
    for (float v : out) m = std::fmax(m, std::fabs(v));
    std::lock_guard<std::mutex> lk(sh.mtx);
    if (m > sh.output_peak) sh.output_peak = m;
    return true;
}

} // namespace sonus

} // namespace ergo::audio
