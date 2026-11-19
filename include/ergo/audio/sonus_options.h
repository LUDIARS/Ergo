#pragma once

/// ergo::audio::sonus — settings and counters of ergo_audio's **Sonus** backend
/// (LUDIARS Sonus, `-DERGO_AUDIO_BACKEND=sonus -DERGO_SONUS_DIR=<checkout>`).
///
/// The functions exist only in that build: guard calls with
/// `#if defined(ERGO_AUDIO_BACKEND_SONUS)` (ergo_audio defines it for its users).
/// No Sonus type appears here; the backend keeps Sonus behind ergo_audio.
/// See spec/module/audio_sonus.md.

#include <cstdint>

namespace ergo::audio::sonus {

/// Where the mix goes (Sonus' built-in devices, snd_config.output_kind).
enum class Output : uint8_t {
    Platform = 0,  ///< the OS default device (WASAPI / Core Audio / ALSA). No device -> initialize() fails.
    Silent   = 1,  ///< Sonus' silent device, running in real time on its own thread (CI, headless servers).
    Manual   = 2,  ///< Sonus' silent device that only advances in advance_output() (tests, offline hosts).
};

struct Options {
    Output   output               = Output::Platform;
    /// Open Sonus' live link so Ergo Studio's sonus plugin (or the Sonus tool) can
    /// connect: mixer, profiler, recording. Development builds only.
    bool     live_link            = false;
    /// TCP port of the live link; 0 = Sonus' own default.
    uint16_t live_link_port       = 0;
    /// Sounds loaded at once (each is one Sonus static buffer; a sound unloaded counts until
    /// Sonus releases it), 1..4096. A load beyond it fails and is logged. Plays of a sound
    /// are not limited by it (Sonus plays one buffer any number of times at once).
    uint32_t max_loaded_sounds    = 256;
};

/// Set before Engine::initialize(). Returns false (and logs why) when the engine is
/// already initialized, a value is out of range, or `live_link` is asked of a Sonus
/// built without its live link (SND_LIVELINK=OFF). The options are kept otherwise.
bool    configure(const Options& options);
Options options();

struct Stats {
    bool     running              = false;  ///< initialized
    uint32_t plays_active         = 0;      ///< plays started and not ended yet
    uint32_t sounds_held          = 0;      ///< Sonus buffers whose samples are held (loaded or being released)
    uint64_t blocks_rendered      = 0;      ///< Sonus blocks since initialize()
    float    master_peak          = 0.0f;   ///< linear peak of the master bus (latest meter)
    float    output_peak          = 0.0f;   ///< Output::Manual: largest |sample| advance_output() delivered
    uint16_t live_link_port       = 0;      ///< 0 = no live link
    uint32_t dropped_plays        = 0;      ///< plays that could not start (no buffer / playback slot)
};
/// Counters as of the latest Engine::update() (output_peak: of the latest advance_output()).
/// Callable from any thread.
Stats stats();
/// Output::Manual: start a new output_peak measurement.
void  reset_output_peak();
/// Output::Manual: play `periods` device periods (Sonus' block size each) now, on the
/// thread that called Engine::initialize(). Sonus renders ahead of the device, so call
/// Engine::update() first for commands to reach it. false (and logged once) when the
/// engine is not initialized with Output::Manual, on another thread, or Sonus refuses.
bool  advance_output(uint32_t periods);

} // namespace ergo::audio::sonus
