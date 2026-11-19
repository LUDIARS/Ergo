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

/// Where the mix goes.
enum class Output : uint8_t {
    Platform = 0,  ///< the OS default device (WASAPI / Core Audio / ALSA). No device -> initialize() fails.
    Silent   = 1,  ///< a timer that drops the samples (CI, headless servers, tests). Never chosen automatically.
};

struct Options {
    Output   output               = Output::Platform;
    /// Open Sonus' live link so Ergo Studio's sonus plugin (or the Sonus tool) can
    /// connect: mixer, profiler, recording. Development builds only.
    bool     live_link            = false;
    /// TCP port of the live link; 0 = Sonus' own default.
    uint16_t live_link_port       = 0;
    /// Plays that may sound (or wait for Sonus to release their samples) at once, 1..4096.
    /// A play beyond it is dropped and logged.
    uint32_t max_concurrent_plays = 128;
};

/// Set before Engine::initialize(). Returns false (and logs why) when the engine is
/// already initialized, a value is out of range, or `live_link` is asked of a Sonus
/// built without its live link (SND_LIVELINK=OFF). The options are kept otherwise.
bool    configure(const Options& options);
Options options();

struct Stats {
    bool     running              = false;  ///< initialized
    uint32_t plays_held           = 0;      ///< plays whose samples Sonus still references
    uint64_t blocks_rendered      = 0;      ///< Sonus blocks since initialize()
    float    master_peak          = 0.0f;   ///< linear peak of the master bus (latest meter)
    float    output_peak          = 0.0f;   ///< Output::Silent: largest |sample| sent to the device so far
    uint16_t live_link_port       = 0;      ///< 0 = no live link
    uint32_t dropped_plays        = 0;      ///< plays that could not start (no free buffer / slot)
};
/// Counters as of the latest Engine::update(). Callable from any thread.
Stats stats();
/// Output::Silent: start a new output_peak measurement.
void  reset_output_peak();

} // namespace ergo::audio::sonus
