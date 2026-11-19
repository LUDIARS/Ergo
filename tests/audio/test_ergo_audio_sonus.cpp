// ergo_audio's Sonus backend (spec/module/audio_sonus.md). Hermetic: the WAV
// files are generated here into a private temp directory, the output is Sonus'
// silent device of the backend (no sound card), and the live link listens on a
// loopback port picked per process.
#include "gtest/gtest.h"

#include "ergo/audio/audio_engine.h"
#include "ergo/audio/sonus_options.h"
#include "wav_pcm.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#define ERGO_TEST_GETPID _getpid
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define ERGO_TEST_GETPID getpid
#endif

using namespace ergo::audio;
namespace fs = std::filesystem;

namespace {

// ---- WAV writing (the test's own fixtures) ---------------------------------------------
void put16(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>(v));
    b.push_back(static_cast<uint8_t>(v >> 8));
}
void put32(std::vector<uint8_t>& b, uint32_t v) {
    put16(b, v & 0xFFFFu);
    put16(b, v >> 16);
}

/// A RIFF WAVE with `format` (1 PCM, 3 float, 0xFFFE extensible over `sub`) and raw sample bytes.
std::vector<uint8_t> wav_bytes(uint16_t format, uint16_t channels, uint32_t rate, uint16_t bits,
                               const std::vector<uint8_t>& data, uint16_t sub = 1) {
    std::vector<uint8_t> fmt;
    put16(fmt, format);
    put16(fmt, channels);
    put32(fmt, rate);
    put32(fmt, rate * channels * (bits / 8u));
    put16(fmt, static_cast<uint16_t>(channels * (bits / 8u)));
    put16(fmt, bits);
    if (format == 0xFFFE) {
        put16(fmt, 22);
        put16(fmt, bits);
        put32(fmt, 0);
        put16(fmt, sub);   // sub-format GUID: tag first, then the fixed KSDATAFORMAT tail
        const uint8_t tail[14] = {0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
        fmt.insert(fmt.end(), tail, tail + 14);
    }
    std::vector<uint8_t> b = {'R', 'I', 'F', 'F'};
    put32(b, static_cast<uint32_t>(4 + 8 + fmt.size() + 8 + data.size()));
    b.insert(b.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put32(b, static_cast<uint32_t>(fmt.size()));
    b.insert(b.end(), fmt.begin(), fmt.end());
    b.insert(b.end(), {'d', 'a', 't', 'a'});
    put32(b, static_cast<uint32_t>(data.size()));
    b.insert(b.end(), data.begin(), data.end());
    return b;
}

/// Mono float32 WAV holding `frames` samples of `value` (a DC level: its peak is exact).
std::vector<uint8_t> dc_wav(float value, uint32_t frames, uint32_t rate = 48000) {
    std::vector<uint8_t> data;
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    for (uint32_t i = 0; i < frames; ++i) put32(data, bits);
    return wav_bytes(3, 1, rate, 32, data);
}

class TempDir {
public:
    TempDir() {
        static int n = 0;
        std::error_code ec;
        dir_ = fs::temp_directory_path(ec) /
               ("ergo-audio-sonus-" + std::to_string(ERGO_TEST_GETPID()) + "-" + std::to_string(++n));
        fs::remove_all(dir_, ec);
        fs::create_directories(dir_, ec);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }
    std::string write(const std::string& name, const std::vector<uint8_t>& bytes) const {
        const fs::path p = dir_ / name;
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return p.string();
    }

private:
    fs::path dir_;
};

sonus::Options silent_options() {
    sonus::Options o;
    o.output = sonus::Output::Silent;
    return o;
}

/// update() every millisecond until pred() or `seconds`.
template <class Pred>
bool pump_until(Pred pred, double seconds = 30.0) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
    for (;;) {
        Engine::instance().update();
        if (pred()) return true;
        if (std::chrono::steady_clock::now() > end) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

/// Plays `h` and returns the largest output sample until the play is reclaimed, or -1
/// when the play did not start or did not end within the time limit.
float play_and_measure(SoundHandle h, float volume, float pitch, uint64_t* blocks = nullptr) {
    pump_until([] { return sonus::stats().plays_held == 0; });
    sonus::reset_output_peak();
    const uint64_t start = sonus::stats().blocks_rendered;
    Engine::instance().play(h, volume, pitch);
    Engine::instance().update();   // starts the queued play
    if (sonus::stats().plays_held != 1u) return -1.0f;
    if (!pump_until([] { return sonus::stats().plays_held == 0; })) return -1.0f;
    if (blocks) *blocks = sonus::stats().blocks_rendered - start;
    return sonus::stats().output_peak;
}

} // namespace

// ---- WAV decoding --------------------------------------------------------------------
TEST(SonusWav, DecodesPcmAndFloatFormats) {
    // 16-bit stereo: L = 0x4000 (0.5), R = 0xC000 (-0.5), two frames.
    const std::vector<uint8_t> d16 = {0x00, 0x40, 0x00, 0xC0, 0x00, 0x40, 0x00, 0xC0};
    auto w = wav_bytes(1, 2, 44100, 16, d16);
    auto r = detail::decode_wav(w.data(), w.size());
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.pcm.channels, 2u);
    EXPECT_EQ(r.pcm.sample_rate, 44100u);
    EXPECT_EQ(r.pcm.frames, 2u);
    EXPECT_FLOAT_EQ(r.pcm.samples[0], 0.5f);    // planar: L0 L1 R0 R1
    EXPECT_FLOAT_EQ(r.pcm.samples[1], 0.5f);
    EXPECT_FLOAT_EQ(r.pcm.samples[2], -0.5f);

    // 8-bit unsigned, 24-bit and 32-bit signed.
    const std::vector<uint8_t> d8 = {0xC0};
    w = wav_bytes(1, 1, 8000, 8, d8);
    r = detail::decode_wav(w.data(), w.size());
    ASSERT_TRUE(r.ok);
    EXPECT_FLOAT_EQ(r.pcm.samples[0], 0.5f);
    const std::vector<uint8_t> d24 = {0x00, 0x00, 0xC0};
    w = wav_bytes(1, 1, 8000, 24, d24);
    r = detail::decode_wav(w.data(), w.size());
    ASSERT_TRUE(r.ok);
    EXPECT_FLOAT_EQ(r.pcm.samples[0], -0.5f);
    const std::vector<uint8_t> d32 = {0x00, 0x00, 0x00, 0x40};
    w = wav_bytes(1, 1, 8000, 32, d32);
    r = detail::decode_wav(w.data(), w.size());
    ASSERT_TRUE(r.ok);
    EXPECT_FLOAT_EQ(r.pcm.samples[0], 0.5f);

    // float32 and WAVE_FORMAT_EXTENSIBLE (float sub-format).
    w = dc_wav(0.25f, 3);
    r = detail::decode_wav(w.data(), w.size());
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.pcm.frames, 3u);
    EXPECT_FLOAT_EQ(r.pcm.samples[2], 0.25f);
    std::vector<uint8_t> f32(4);
    const float q = -0.75f;
    std::memcpy(f32.data(), &q, 4);
    w = wav_bytes(0xFFFE, 1, 48000, 32, f32, 3);
    r = detail::decode_wav(w.data(), w.size());
    ASSERT_TRUE(r.ok);
    EXPECT_FLOAT_EQ(r.pcm.samples[0], -0.75f);
}

TEST(SonusWav, RejectsWhatItCannotPlay) {
    const std::vector<uint8_t> ogg = {'O', 'g', 'g', 'S', 0, 2, 0, 0, 0, 0, 0, 0, 0, 0};
    auto r = detail::decode_wav(ogg.data(), ogg.size());
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("WAV only"), std::string::npos);

    const std::vector<uint8_t> adpcm_data = {0, 0, 0, 0};
    auto w = wav_bytes(2, 1, 8000, 16, adpcm_data);   // MS ADPCM tag
    r = detail::decode_wav(w.data(), w.size());
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("compressed"), std::string::npos);

    w = wav_bytes(1, 1, 8000, 12, adpcm_data);
    EXPECT_FALSE(detail::decode_wav(w.data(), w.size()).ok);   // 12-bit
    w = wav_bytes(1, 9, 8000, 16, std::vector<uint8_t>(18, 0));
    EXPECT_FALSE(detail::decode_wav(w.data(), w.size()).ok);   // 9 channels
    w = wav_bytes(1, 1, 8000, 16, {});
    EXPECT_FALSE(detail::decode_wav(w.data(), w.size()).ok);   // no samples
    w = dc_wav(0.5f, 4);
    w.resize(20);                                              // cut inside fmt
    EXPECT_FALSE(detail::decode_wav(w.data(), w.size()).ok);
    EXPECT_FALSE(detail::load_wav_file("/nonexistent/ergo-audio-sonus.wav").ok);
}

// ---- options -------------------------------------------------------------------------
TEST(SonusEngine, OptionsAreCheckedAndOnlyTakenBeforeInitialize) {
    auto& e = Engine::instance();
    e.shutdown();
    sonus::Options bad = silent_options();
    bad.max_concurrent_plays = 0;
    EXPECT_FALSE(sonus::configure(bad));
    bad.max_concurrent_plays = 5000;
    EXPECT_FALSE(sonus::configure(bad));
    ASSERT_TRUE(sonus::configure(silent_options()));
    EXPECT_TRUE(sonus::options().output == sonus::Output::Silent);

    ASSERT_TRUE(e.initialize());
    EXPECT_TRUE(e.backend() == Backend::Sonus);
    EXPECT_EQ(std::string(e.backend_name()), "Sonus");
    EXPECT_TRUE(sonus::stats().running);
    EXPECT_EQ(sonus::stats().live_link_port, 0u);
    sonus::Options other = silent_options();
    other.max_concurrent_plays = 7;
    EXPECT_FALSE(sonus::configure(other));          // refused while initialized
    EXPECT_EQ(sonus::options().max_concurrent_plays, 128u);
    e.shutdown();
    EXPECT_FALSE(sonus::stats().running);
}

// ---- playback ------------------------------------------------------------------------
TEST(SonusEngine, PlaysThroughTheSilentDeviceWithVolume) {
    TempDir tmp;
    const std::string dc = tmp.write("dc.wav", dc_wav(0.5f, 4800));
    auto& e = Engine::instance();
    e.shutdown();
    ASSERT_TRUE(sonus::configure(silent_options()));
    ASSERT_TRUE(e.initialize());
    ASSERT_TRUE(pump_until([] { return sonus::stats().blocks_rendered > 4; }));   // the device runs
    EXPECT_FLOAT_EQ(sonus::stats().output_peak, 0.0f);                           // nothing played yet

    const SoundHandle h = e.load_sound(dc);
    ASSERT_NE(h, INVALID_SOUND);
    const float full = play_and_measure(h, 1.0f, 1.0f);
    EXPECT_GT(full, 0.1f);
    const float half = play_and_measure(h, 0.5f, 1.0f);
    EXPECT_NEAR(half, full * 0.5f, full * 0.01f);
    // Out-of-range volumes are clamped like the FMOD backend: > 1 plays at 1, < 0 is silent.
    EXPECT_NEAR(play_and_measure(h, 7.0f, 1.0f), full, full * 0.01f);
    EXPECT_FLOAT_EQ(play_and_measure(h, -3.0f, 1.0f), 0.0f);

    // A play keeps its samples when its sound is unloaded meanwhile.
    e.play(h, 1.0f, 1.0f);
    e.unload_sound(h);
    e.update();
    EXPECT_EQ(sonus::stats().plays_held, 1u);
    ASSERT_TRUE(pump_until([] { return sonus::stats().plays_held == 0; }));
    e.play(h, 1.0f, 1.0f);   // unloaded: nothing queued
    e.update();
    EXPECT_EQ(sonus::stats().plays_held, 0u);
    e.shutdown();
}

TEST(SonusEngine, PitchChangesTheLengthOfAPlay) {
    TempDir tmp;
    const std::string dc = tmp.write("dc.wav", dc_wav(0.5f, 24000));   // 0.5 s at 48 kHz
    auto& e = Engine::instance();
    e.shutdown();
    ASSERT_TRUE(sonus::configure(silent_options()));
    ASSERT_TRUE(e.initialize());
    const SoundHandle h = e.load_sound(dc);
    ASSERT_NE(h, INVALID_SOUND);
    uint64_t normal = 0, fast = 0, slow = 0;
    EXPECT_GT(play_and_measure(h, 1.0f, 1.0f, &normal), 0.0f);
    EXPECT_GT(play_and_measure(h, 1.0f, 2.0f, &fast), 0.0f);
    EXPECT_GT(play_and_measure(h, 1.0f, 0.5f, &slow), 0.0f);
    // 24000 frames = 93.75 blocks of 256; reclaiming adds a few blocks to each.
    EXPECT_GT(normal, 90u);
    EXPECT_LT(normal, 130u);
    EXPECT_NEAR(static_cast<double>(fast), static_cast<double>(normal) / 2.0, 20.0);
    EXPECT_NEAR(static_cast<double>(slow), static_cast<double>(normal) * 2.0, 25.0);
    e.shutdown();
}

TEST(SonusEngine, FailuresAreValuesNotCrashes) {
    TempDir tmp;
    const std::string ogg = tmp.write("music.ogg", {'O', 'g', 'g', 'S', 0, 2, 0, 0, 0, 0, 0, 0, 0, 0});
    const std::string dc  = tmp.write("dc.wav", dc_wav(0.5f, 48000));
    auto& e = Engine::instance();
    e.shutdown();
    EXPECT_EQ(e.load_sound(dc), INVALID_SOUND);   // not initialized

    sonus::Options o = silent_options();
    o.max_concurrent_plays = 2;
    ASSERT_TRUE(sonus::configure(o));
    ASSERT_TRUE(e.initialize());
    EXPECT_EQ(e.load_sound(ogg), INVALID_SOUND);   // not WAV: an explicit failure
    EXPECT_EQ(e.load_sound(tmp.write("missing-dir/none.wav", {})), INVALID_SOUND);
    EXPECT_EQ(e.load_sound("/nonexistent/ergo.wav"), INVALID_SOUND);
    const SoundHandle h = e.load_sound(dc);
    ASSERT_NE(h, INVALID_SOUND);

    // Five plays at once with room for two: three are dropped (and logged).
    for (int i = 0; i < 5; ++i) e.play(h);
    e.update();
    EXPECT_EQ(sonus::stats().plays_held, 2u);
    EXPECT_EQ(sonus::stats().dropped_plays, 3u);
    e.play(9999);            // unknown handle
    e.play(INVALID_SOUND);
    e.unload_sound(9999);
    e.update();
    EXPECT_EQ(sonus::stats().plays_held, 2u);
    e.shutdown();            // frees the plays still sounding
    EXPECT_EQ(sonus::stats().plays_held, 0u);
    ASSERT_TRUE(sonus::configure(silent_options()));
}

TEST(SonusEngine, UpdateFromAnotherThreadIsRefused) {
    TempDir tmp;
    const std::string dc = tmp.write("dc.wav", dc_wav(0.5f, 4800));
    auto& e = Engine::instance();
    e.shutdown();
    ASSERT_TRUE(sonus::configure(silent_options()));
    ASSERT_TRUE(e.initialize());
    const SoundHandle h = e.load_sound(dc);
    // play() is fine from any thread; update() only on the initializing thread.
    std::thread other([&] {
        e.play(h);
        e.update();   // logged and ignored
        e.shutdown(); // logged and ignored
    });
    other.join();
    EXPECT_TRUE(e.is_initialized());
    EXPECT_EQ(sonus::stats().plays_held, 0u);   // the other thread's update() started nothing
    e.update();
    EXPECT_EQ(sonus::stats().plays_held, 1u);
    e.shutdown();
    EXPECT_FALSE(e.is_initialized());
}

#if defined(ERGO_AUDIO_SONUS_TEST_LIVELINK) && !defined(_WIN32)
TEST(SonusEngine, LiveLinkListensOnTheConfiguredPort) {
    auto& e = Engine::instance();
    e.shutdown();
    sonus::Options o = silent_options();
    o.live_link = true;
    bool up = false;
    uint16_t port = 0;
    // A free port near a per-process base (like Sonus' own tests).
    for (int i = 0; i < 32 && !up; ++i) {
        port        = static_cast<uint16_t>(30000 + (ERGO_TEST_GETPID() * 7 + i * 101) % 20000);
        o.live_link_port = port;
        ASSERT_TRUE(sonus::configure(o));
        up = e.initialize();
    }
    ASSERT_TRUE(up);
    EXPECT_EQ(sonus::stats().live_link_port, port);
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(fd >= 0);
    sockaddr_in a{};
    a.sin_family      = AF_INET;
    a.sin_port        = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    EXPECT_EQ(::connect(fd, reinterpret_cast<const sockaddr*>(&a), sizeof a), 0);
    ::close(fd);
    e.shutdown();
    ASSERT_TRUE(sonus::configure(silent_options()));
}
#endif
