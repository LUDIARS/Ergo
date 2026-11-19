#pragma once
/// RIFF WAVE -> float PCM, for ergo_audio backends that do not decode files
/// themselves (the Sonus backend plays game-owned sample memory). Backend
/// independent; no third-party code.
///
/// Accepted: WAVE_FORMAT_PCM 8 (unsigned) / 16 / 24 / 32-bit, WAVE_FORMAT_IEEE_FLOAT
/// 32 / 64-bit, and WAVE_FORMAT_EXTENSIBLE with one of those sub-formats;
/// 1..8 channels; any sample rate above 0. Everything else (compressed WAV,
/// OGG, MP3, FLAC, a truncated file) is an explicit error, never silence.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ergo::audio::detail {

inline constexpr uint32_t kWavMaxChannels = 8;

/// Planar float samples: channel `c` is samples[c * frames .. (c + 1) * frames).
struct PcmSound {
    uint32_t           channels    = 0;
    uint32_t           sample_rate = 0;
    uint32_t           frames      = 0;
    std::vector<float> samples;
};

/// `ok` false: `error` says why (no file name; the caller adds it).
struct PcmResult {
    bool        ok = false;
    std::string error;
    PcmSound    pcm;
};

PcmResult decode_wav(const uint8_t* data, std::size_t size);
PcmResult load_wav_file(const std::string& path);

} // namespace ergo::audio::detail
