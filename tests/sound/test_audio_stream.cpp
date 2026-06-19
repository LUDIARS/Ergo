// AudioStream テスト

#include <gtest/gtest.h>
#include "ergo/sound/audio_stream.h"
#include <fstream>
#include <cmath>
#include <filesystem>

using namespace ergo::sound;

namespace {

std::string tmpPath(const char* filename) {
    return (std::filesystem::temp_directory_path() / filename).string();
}


std::string createTestWav(const std::string& path, uint32_t numFrames = 4410) {
    std::ofstream file(path, std::ios::binary);
    uint16_t channels = 1;
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
    }
    file.close();
    return path;
}

} // namespace

class AudioStreamTest : public ::testing::Test {
protected:
    void SetUp() override {
        wavPath_ = tmpPath("ergo_stream_test.wav");
        createTestWav(wavPath_, 10000);
    }

    void TearDown() override {
        std::remove(wavPath_.c_str());
    }

    std::string wavPath_;
};

// ストリームのオープン
TEST_F(AudioStreamTest, Open) {
    AudioStream stream;
    ASSERT_TRUE(stream.open(wavPath_));
    EXPECT_TRUE(stream.isOpen());
    EXPECT_FALSE(stream.isEndOfStream());
    EXPECT_EQ(stream.getTotalFrames(), 10000u);
    stream.close();
}

// 部分的なデータ読み出し
TEST_F(AudioStreamTest, PartialRead) {
    AudioStream stream(1024);  // 小さなバッファ
    ASSERT_TRUE(stream.open(wavPath_));

    std::vector<float> buffer(500);
    size_t read = stream.read(buffer.data(), 500);
    EXPECT_EQ(read, 500u);
    EXPECT_FALSE(stream.isEndOfStream());

    stream.close();
}

// ストリーム全体の読み出し
TEST_F(AudioStreamTest, ReadAll) {
    AudioStream stream(1024);
    ASSERT_TRUE(stream.open(wavPath_));

    std::vector<float> buffer(10000);
    size_t totalRead = 0;
    while (!stream.isEndOfStream()) {
        size_t read = stream.read(buffer.data() + totalRead, 1000);
        totalRead += read;
        if (read == 0) break;
    }

    EXPECT_EQ(totalRead, 10000u);
    EXPECT_TRUE(stream.isEndOfStream());

    stream.close();
}

// ストリーム終端が正しく検出されること
TEST_F(AudioStreamTest, EndOfStream) {
    AudioStream stream;
    ASSERT_TRUE(stream.open(wavPath_));

    std::vector<float> buffer(20000);  // ファイルより大きいバッファ
    size_t read = stream.read(buffer.data(), 20000);
    EXPECT_EQ(read, 10000u);
    EXPECT_TRUE(stream.isEndOfStream());

    stream.close();
}

// シークが正しく動作すること
TEST_F(AudioStreamTest, Seek) {
    AudioStream stream;
    ASSERT_TRUE(stream.open(wavPath_));

    // 中間にシーク
    ASSERT_TRUE(stream.seek(5000));

    std::vector<float> buffer(5000);
    size_t read = stream.read(buffer.data(), 5000);
    EXPECT_EQ(read, 5000u);

    // 残り0フレームの読み出しでEOSが確定する
    std::vector<float> extra(1);
    stream.read(extra.data(), 1);
    EXPECT_TRUE(stream.isEndOfStream());

    // 先頭に戻る
    ASSERT_TRUE(stream.seek(0));
    EXPECT_FALSE(stream.isEndOfStream());

    stream.close();
}

// 存在しないファイル
TEST_F(AudioStreamTest, NonexistentFile) {
    AudioStream stream;
    EXPECT_FALSE(stream.open(tmpPath("ergo_nonexistent_stream.wav")));
    EXPECT_FALSE(stream.isOpen());
}

// フォーマット情報の取得
TEST_F(AudioStreamTest, GetFormat) {
    AudioStream stream;
    ASSERT_TRUE(stream.open(wavPath_));

    auto fmt = stream.getFormat();
    EXPECT_EQ(fmt.sampleRate, 44100u);
    EXPECT_EQ(fmt.channels, 1);

    stream.close();
}
