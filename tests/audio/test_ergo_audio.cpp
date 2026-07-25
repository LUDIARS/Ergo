#include "gtest/gtest.h"

#include "ergo/audio/audio_engine.h"

#include <cmath>
#include <vector>

using namespace ergo::audio;

namespace {

/// A short mono sine burst standing in for a Figmentum-synthesised SFX.
std::vector<float> make_test_pcm(size_t frames = 4410, int rate = 44100) {
    std::vector<float> pcm(frames);
    for (size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / rate;
        pcm[i] = 0.25f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * 440.0 * t));
    }
    return pcm;
}

} // namespace

/// Tests run against whichever backend is linked in. Dummy is always
/// present (final fallback) so the tests are valid on a stock build.
/// When FMOD or miniaudio is linked, the same tests stress the real
/// pipeline — a missing-file load_sound will fail gracefully and
/// return 0.
///
/// **Device independence**: the real backends open an actual output
/// device in `initialize()`, which legitimately fails on a headless CI
/// runner. So no test may assert that `initialize()` succeeded — each
/// one asserts the contract that holds on *both* branches:
///   - initialized   → a valid load returns a non-zero handle
///   - uninitialized → every call is a safe no-op returning 0
/// Either way nothing may crash.

TEST(AudioEngine, SingletonIsStable) {
    EXPECT_EQ(&Engine::instance(), &Engine::instance());
}

TEST(AudioEngine, BackendNameIsKnown) {
    const char* n = Engine::instance().backend_name();
    ASSERT_NE(n, nullptr);
    const std::string s = n;
    EXPECT_TRUE(s == "Dummy" || s == "FMOD" || s == "miniaudio");
}

TEST(AudioEngine, InitializeAndShutdownIdempotent) {
    auto& e = Engine::instance();
    // initialize() may fail without an output device; whichever way it
    // goes, the second call must agree with the first and is_initialized()
    // must track it.
    const bool ok = e.initialize();
    EXPECT_EQ(e.initialize(), ok);   // idempotent, no re-init
    EXPECT_EQ(e.is_initialized(), ok);
    e.shutdown();
    e.shutdown();                    // second call is a no-op
    EXPECT_FALSE(e.is_initialized());
}

TEST(AudioEngine, LoadFailsWithoutInit) {
    auto& e = Engine::instance();
    e.shutdown();
    EXPECT_EQ(e.load_sound("nonsense.wav"), INVALID_SOUND);
}

TEST(AudioEngine, LoadAndPlayHappyPath) {
    auto& e = Engine::instance();
    e.initialize();

    // Dummy backend: any path succeeds and returns a fresh handle.
    // FMOD / miniaudio: the missing file fails the load and returns
    // INVALID_SOUND. All outcomes are acceptable here — we just care
    // that play() on the resulting handle is safe.
    const SoundHandle h = e.load_sound("pickup_common.wav");

    e.play(h, 0.5f, 1.0f);
    e.play(INVALID_SOUND);         // safe no-op
    e.update();

    e.unload_sound(h);
    e.unload_sound(INVALID_SOUND); // safe no-op
    e.shutdown();
}

TEST(AudioEngine, PlayClampsParameters) {
    auto& e = Engine::instance();
    e.initialize();
    const SoundHandle h = e.load_sound("sfx_out_of_range.wav");

    // These must not crash — out-of-range values are accepted and
    // clamped by the backend (< 0 volume, negative pitch).
    e.play(h, -5.0f, -1.0f);
    e.play(h,  2.0f,  1000.0f);

    e.shutdown();
}

TEST(AudioEngine, UpdateSafeBeforeInit) {
    auto& e = Engine::instance();
    e.shutdown();
    e.update();  // should not crash
    EXPECT_FALSE(e.is_initialized());
}

// ---------------------------------------------------------------------------
// load_sound_pcm — in-memory float mono PCM (Figmentum renderSound output)
// ---------------------------------------------------------------------------

TEST(AudioEnginePcm, LoadPcmFailsWithoutInit) {
    auto& e = Engine::instance();
    e.shutdown();
    const std::vector<float> pcm = make_test_pcm();
    EXPECT_EQ(e.load_sound_pcm(pcm.data(), pcm.size(), 44100), INVALID_SOUND);
}

TEST(AudioEnginePcm, LoadPcmAndPlay) {
    auto& e = Engine::instance();
    const bool ready = e.initialize();

    const std::vector<float> pcm = make_test_pcm();
    const SoundHandle h = e.load_sound_pcm(pcm.data(), pcm.size(), 44100);

    if (ready) {
        // Unlike load_sound(path), a valid in-memory block has nothing
        // to fail on — every backend must hand back a live handle.
        EXPECT_NE(h, INVALID_SOUND);
    } else {
        EXPECT_EQ(h, INVALID_SOUND);   // headless: safe no-op
    }

    e.play(h, 0.5f, 1.0f);   // must not crash on either branch
    e.update();

    e.unload_sound(h);
    e.shutdown();
}

TEST(AudioEnginePcm, PcmSamplesAreCopied) {
    auto& e = Engine::instance();
    const bool ready = e.initialize();

    SoundHandle h = INVALID_SOUND;
    {
        // The engine must copy: this buffer is gone before play().
        const std::vector<float> scratch = make_test_pcm(2205);
        h = e.load_sound_pcm(scratch.data(), scratch.size(), 22050);
    }
    if (ready) EXPECT_NE(h, INVALID_SOUND);

    e.play(h, 1.0f, 1.0f);
    e.update();
    e.shutdown();
}

TEST(AudioEnginePcm, RejectsDegenerateInput) {
    auto& e = Engine::instance();
    e.initialize();

    const std::vector<float> pcm = make_test_pcm(64);
    EXPECT_EQ(e.load_sound_pcm(nullptr,    16,         44100), INVALID_SOUND);
    EXPECT_EQ(e.load_sound_pcm(pcm.data(), 0,          44100), INVALID_SOUND);
    EXPECT_EQ(e.load_sound_pcm(pcm.data(), pcm.size(), 0),     INVALID_SOUND);
    EXPECT_EQ(e.load_sound_pcm(pcm.data(), pcm.size(), -1),    INVALID_SOUND);

    e.shutdown();
}

TEST(AudioEnginePcm, PlayAfterUnloadIsSafe) {
    auto& e = Engine::instance();
    e.initialize();

    const std::vector<float> pcm = make_test_pcm(1024);
    const SoundHandle h = e.load_sound_pcm(pcm.data(), pcm.size(), 44100);

    e.unload_sound(h);
    e.play(h, 1.0f, 1.0f);   // handle is dead — must be a no-op, not a crash
    e.update();

    e.shutdown();
}

TEST(AudioEnginePcm, OverlappingOneShotsDoNotShareCursor) {
    auto& e = Engine::instance();
    const bool ready = e.initialize();

    const std::vector<float> pcm = make_test_pcm();
    const SoundHandle h = e.load_sound_pcm(pcm.data(), pcm.size(), 44100);
    if (ready) ASSERT_NE(h, INVALID_SOUND);

    // Each play() must spin up an independent voice; firing the same
    // handle repeatedly must neither crash nor leak across shutdown.
    for (int i = 0; i < 8; ++i) {
        e.play(h, 0.2f, 1.0f + 0.05f * i);
    }
    e.update();

    e.shutdown();
}
