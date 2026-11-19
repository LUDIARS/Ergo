#include "wav_pcm.h"

#include <cstring>
#include <fstream>
#include <iterator>

namespace ergo::audio::detail {
namespace {

constexpr uint16_t kFormatPcm        = 0x0001;
constexpr uint16_t kFormatFloat      = 0x0003;
constexpr uint16_t kFormatExtensible = 0xFFFE;

uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

PcmResult fail(std::string why) {
    PcmResult r;
    r.error = std::move(why);
    return r;
}

/// One sample at `p` as float in [-1, 1].
float sample_at(const uint8_t* p, uint16_t format, uint16_t bits) {
    if (format == kFormatFloat) {
        if (bits == 32) {
            float f;
            uint32_t v = u32(p);
            std::memcpy(&f, &v, sizeof f);
            return f;
        }
        uint64_t v = static_cast<uint64_t>(u32(p)) | (static_cast<uint64_t>(u32(p + 4)) << 32);
        double d;
        std::memcpy(&d, &v, sizeof d);
        return static_cast<float>(d);
    }
    switch (bits) {
    case 8: return (static_cast<float>(p[0]) - 128.0f) / 128.0f;
    case 16: return static_cast<float>(static_cast<int16_t>(u16(p))) / 32768.0f;
    case 24: {
        int32_t v = static_cast<int32_t>((static_cast<uint32_t>(p[0]) << 8) | (static_cast<uint32_t>(p[1]) << 16) |
                                         (static_cast<uint32_t>(p[2]) << 24));
        return static_cast<float>(v >> 8) / 8388608.0f;
    }
    default: return static_cast<float>(static_cast<int32_t>(u32(p))) / 2147483648.0f;
    }
}

} // namespace

PcmResult decode_wav(const uint8_t* data, std::size_t size) {
    if (size < 12 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
        return fail("not a RIFF WAVE file (this backend decodes WAV only; OGG / MP3 / FLAC are not supported)");
    }
    const uint8_t* fmt       = nullptr;
    uint32_t       fmt_size  = 0;
    const uint8_t* body      = nullptr;
    uint32_t       body_size = 0;
    std::size_t    at        = 12;
    while (at + 8 <= size) {
        const uint8_t*    id    = data + at;
        const uint32_t    len   = u32(data + at + 4);
        const std::size_t start = at + 8;
        if (len > size - start) {
            // A truncated "data" chunk is common (streamed writers); take what is there.
            if (std::memcmp(id, "data", 4) != 0) return fail("truncated WAV chunk");
        }
        const uint32_t have = static_cast<uint32_t>(len > size - start ? size - start : len);
        if (std::memcmp(id, "fmt ", 4) == 0) {
            fmt      = data + start;
            fmt_size = have;
        } else if (std::memcmp(id, "data", 4) == 0) {
            body      = data + start;
            body_size = have;
        }
        at = start + have + (have & 1u);   // chunks are word aligned
    }
    if (fmt == nullptr || fmt_size < 16) return fail("WAV has no fmt chunk");
    if (body == nullptr) return fail("WAV has no data chunk");

    uint16_t       format   = u16(fmt);
    const uint16_t channels = u16(fmt + 2);
    const uint32_t rate     = u32(fmt + 4);
    const uint16_t align    = u16(fmt + 12);
    const uint16_t bits     = u16(fmt + 14);
    if (format == kFormatExtensible) {
        if (fmt_size < 40) return fail("WAVE_FORMAT_EXTENSIBLE without its sub-format");
        format = u16(fmt + 24);   // the first two bytes of the sub-format GUID are the format tag
    }
    if (format != kFormatPcm && format != kFormatFloat) {
        return fail("compressed WAV (format tag " + std::to_string(format) + ") is not supported; use PCM or float");
    }
    const bool bits_ok = format == kFormatPcm ? (bits == 8 || bits == 16 || bits == 24 || bits == 32)
                                              : (bits == 32 || bits == 64);
    if (!bits_ok) return fail("unsupported WAV sample size: " + std::to_string(bits) + " bits");
    if (channels == 0 || channels > kWavMaxChannels) {
        return fail("unsupported WAV channel count: " + std::to_string(channels) + " (1.." +
                    std::to_string(kWavMaxChannels) + ")");
    }
    if (rate == 0) return fail("WAV sample rate is 0");
    const uint32_t bytes = bits / 8u;
    if (align != channels * bytes) return fail("inconsistent WAV block align");
    const uint32_t frames = body_size / align;
    if (frames == 0) return fail("WAV has no samples");

    PcmResult r;
    r.ok              = true;
    r.pcm.channels    = channels;
    r.pcm.sample_rate = rate;
    r.pcm.frames      = frames;
    r.pcm.samples.resize(static_cast<std::size_t>(frames) * channels);
    for (uint32_t f = 0; f < frames; ++f) {
        const uint8_t* frame = body + static_cast<std::size_t>(f) * align;
        for (uint32_t c = 0; c < channels; ++c) {
            r.pcm.samples[static_cast<std::size_t>(c) * frames + f] = sample_at(frame + c * bytes, format, bits);
        }
    }
    return r;
}

PcmResult load_wav_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return fail("cannot open the file");
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) return fail("cannot read the file");
    return decode_wav(bytes.data(), bytes.size());
}

} // namespace ergo::audio::detail
