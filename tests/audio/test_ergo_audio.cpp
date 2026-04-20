#include "gtest/gtest.h"

#include "ergo/audio/audio_engine.h"

using namespace ergo::audio;

/// Tests run against whichever backend is linked in. Dummy is always
/// present (fallback when FMOD SDK isn't installed) so the tests are
/// valid on a stock build. When FMOD is available, the same tests
/// stress the real pipeline — a missing-file load_sound will fail
/// gracefully and return 0.

TEST(AudioEngine, SingletonIsStable) {
    EXPECT_EQ(&Engine::instance(), &Engine::instance());
}

TEST(AudioEngine, BackendNameIsKnown) {
    const char* n = Engine::instance().backend_name();
    ASSERT_NE(n, nullptr);
    const std::string s = n;
    EXPECT_TRUE(s == "Dummy" || s == "FMOD");
}

TEST(AudioEngine, InitializeAndShutdownIdempotent) {
    auto& e = Engine::instance();
    EXPECT_TRUE(e.initialize());
    EXPECT_TRUE(e.initialize());   // second call returns true without re-init
    EXPECT_TRUE(e.is_initialized());
    e.shutdown();
    e.shutdown();                  // second call is a no-op
    EXPECT_FALSE(e.is_initialized());
}

TEST(AudioEngine, LoadFailsWithoutInit) {
    auto& e = Engine::instance();
    e.shutdown();
    EXPECT_EQ(e.load_sound("nonsense.wav"), INVALID_SOUND);
}

TEST(AudioEngine, LoadAndPlayHappyPath) {
    auto& e = Engine::instance();
    ASSERT_TRUE(e.initialize());

    // Dummy backend: any path succeeds and returns a fresh handle.
    // FMOD backend: the missing file triggers a failed createSound and
    // returns INVALID_SOUND. Both outcomes are acceptable here — we
    // just care that play() on the resulting handle is safe.
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
    ASSERT_TRUE(e.initialize());
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
