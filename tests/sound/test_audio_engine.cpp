// AudioEngine テスト

#include <gtest/gtest.h>
#include "ergo/sound/audio_engine.h"
#include <fstream>
#include <cmath>
#include <thread>
#include <chrono>

using namespace ergo::sound;

namespace {

std::string createTestWav(const std::string& path, uint32_t numFrames = 4410) {
    std::ofstream file(path, std::ios::binary);
    uint16_t channels = 2;
    uint32_t sampleRate = 44100;
    uint16_t bitsPerSample = 16;
    uint32_t dataSize = numFrames * channels * (bitsPerSample / 8);
    uint32_t fileSize = 36 + dataSize;

    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&fileSize), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    uint32_t fmtSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    uint16_t audioFormat = 1;
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    uint16_t blockAlign = channels * (bitsPerSample / 8);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);

    for (uint32_t f = 0; f < numFrames; ++f) {
        double t = static_cast<double>(f) / static_cast<double>(sampleRate);
        int16_t sample = static_cast<int16_t>(std::sin(2.0 * M_PI * 440.0 * t) * 32767.0);
        file.write(reinterpret_cast<const char*>(&sample), 2);
        file.write(reinterpret_cast<const char*>(&sample), 2);
    }
    file.close();
    return path;
}

} // namespace

class AudioEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        wavPath_ = "/tmp/ergo_engine_test.wav";
        createTestWav(wavPath_, 44100);  // 1秒分
    }

    void TearDown() override {
        std::remove(wavPath_.c_str());
    }

    std::string wavPath_;
};

// エンジンの初期化と終了
TEST_F(AudioEngineTest, InitializeAndShutdown) {
    AudioEngine engine;
    AudioEngineConfig config;
    config.maxVoices = 8;

    ASSERT_TRUE(engine.initialize(config));
    EXPECT_TRUE(engine.isRunning());

    engine.shutdown();
    EXPECT_FALSE(engine.isRunning());
}

// 二重初期化の防止
TEST_F(AudioEngineTest, DoubleInitialize) {
    AudioEngine engine;
    AudioEngineConfig config;

    ASSERT_TRUE(engine.initialize(config));
    EXPECT_FALSE(engine.initialize(config));  // 2回目は失敗

    engine.shutdown();
}

// ボイスの再生
TEST_F(AudioEngineTest, PlayVoice) {
    AudioEngine engine;
    AudioEngineConfig config;
    config.maxVoices = 4;
    ASSERT_TRUE(engine.initialize(config));

    auto stream = std::make_shared<AudioStream>();
    ASSERT_TRUE(stream->open(wavPath_));

    VoiceId id = engine.play(stream);
    EXPECT_NE(id, InvalidVoiceId);
    EXPECT_EQ(engine.getVoiceState(id), VoiceState::Playing);
    EXPECT_EQ(engine.getActiveVoiceCount(), 1u);

    engine.shutdown();
}

// ボイスの停止
TEST_F(AudioEngineTest, StopVoice) {
    AudioEngine engine;
    AudioEngineConfig config;
    ASSERT_TRUE(engine.initialize(config));

    auto stream = std::make_shared<AudioStream>();
    ASSERT_TRUE(stream->open(wavPath_));

    VoiceId id = engine.play(stream);
    engine.stop(id);
    EXPECT_EQ(engine.getVoiceState(id), VoiceState::Stopped);
    EXPECT_EQ(engine.getActiveVoiceCount(), 0u);

    engine.shutdown();
}

// ボイスの一時停止と再開
TEST_F(AudioEngineTest, PauseAndResume) {
    AudioEngine engine;
    AudioEngineConfig config;
    ASSERT_TRUE(engine.initialize(config));

    auto stream = std::make_shared<AudioStream>();
    ASSERT_TRUE(stream->open(wavPath_));

    VoiceId id = engine.play(stream);

    engine.pause(id);
    EXPECT_EQ(engine.getVoiceState(id), VoiceState::Paused);

    engine.resume(id);
    EXPECT_EQ(engine.getVoiceState(id), VoiceState::Playing);

    engine.shutdown();
}

// 全ボイス停止
TEST_F(AudioEngineTest, StopAll) {
    AudioEngine engine;
    AudioEngineConfig config;
    config.maxVoices = 4;
    ASSERT_TRUE(engine.initialize(config));

    for (int i = 0; i < 3; ++i) {
        auto stream = std::make_shared<AudioStream>();
        ASSERT_TRUE(stream->open(wavPath_));
        engine.play(stream);
    }

    EXPECT_EQ(engine.getActiveVoiceCount(), 3u);

    engine.stopAll();
    EXPECT_EQ(engine.getActiveVoiceCount(), 0u);

    engine.shutdown();
}

// マスター音量
TEST_F(AudioEngineTest, MasterVolume) {
    AudioEngine engine;
    AudioEngineConfig config;
    ASSERT_TRUE(engine.initialize(config));

    engine.setMasterVolume(0.5f);
    EXPECT_FLOAT_EQ(engine.getMasterVolume(), 0.5f);

    // クランプ
    engine.setMasterVolume(3.0f);
    EXPECT_FLOAT_EQ(engine.getMasterVolume(), 2.0f);

    engine.setMasterVolume(-1.0f);
    EXPECT_FLOAT_EQ(engine.getMasterVolume(), 0.0f);

    engine.shutdown();
}

// 複数ボイスのミキシング（出力コールバックで確認）
TEST_F(AudioEngineTest, MixMultipleVoices) {
    AudioEngine engine;
    AudioEngineConfig config;
    config.bufferFrames = 256;
    config.maxVoices = 4;
    ASSERT_TRUE(engine.initialize(config));

    std::atomic<int> callbackCount{0};
    engine.setOutputCallback([&](float* buffer, size_t frames, uint16_t channels) {
        callbackCount.fetch_add(1);
    });

    auto stream1 = std::make_shared<AudioStream>();
    auto stream2 = std::make_shared<AudioStream>();
    ASSERT_TRUE(stream1->open(wavPath_));
    ASSERT_TRUE(stream2->open(wavPath_));

    engine.play(stream1, 0.5f);
    engine.play(stream2, 0.5f);

    // 少し待ってコールバックが呼ばれることを確認
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_GT(callbackCount.load(), 0);

    engine.shutdown();
}

// ボイス上限
TEST_F(AudioEngineTest, MaxVoices) {
    AudioEngine engine;
    AudioEngineConfig config;
    config.maxVoices = 2;
    ASSERT_TRUE(engine.initialize(config));

    auto s1 = std::make_shared<AudioStream>();
    auto s2 = std::make_shared<AudioStream>();
    auto s3 = std::make_shared<AudioStream>();
    ASSERT_TRUE(s1->open(wavPath_));
    ASSERT_TRUE(s2->open(wavPath_));
    ASSERT_TRUE(s3->open(wavPath_));

    VoiceId id1 = engine.play(s1);
    VoiceId id2 = engine.play(s2);
    VoiceId id3 = engine.play(s3);  // 上限超過

    EXPECT_NE(id1, InvalidVoiceId);
    EXPECT_NE(id2, InvalidVoiceId);
    EXPECT_EQ(id3, InvalidVoiceId);  // 取得不可

    engine.shutdown();
}
