// AudioDecoder テスト

#include <gtest/gtest.h>
#include "ergo/sound/audio_decoder.h"
#include <fstream>
#include <cstring>
#include <cmath>

using namespace ergo::sound;

namespace {

// テスト用WAVファイルを生成するヘルパー
// 440Hz正弦波、16bit、モノラル、44100Hz、指定フレーム数
std::string createTestWavFile(const std::string& path, uint32_t sampleRate = 44100,
                               uint16_t channels = 1, uint16_t bitsPerSample = 16,
                               uint32_t numFrames = 4410) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return "";

    uint32_t bytesPerSample = bitsPerSample / 8;
    uint32_t dataSize = numFrames * channels * bytesPerSample;
    uint32_t fileSize = 36 + dataSize;

    // RIFF header
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&fileSize), 4);
    file.write("WAVE", 4);

    // fmt subchunk
    file.write("fmt ", 4);
    uint32_t fmtSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    uint16_t audioFormat = 1; // PCM
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    uint32_t byteRate = sampleRate * channels * bytesPerSample;
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    uint16_t blockAlign = channels * bytesPerSample;
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data subchunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);

    // 440Hz正弦波データ
    for (uint32_t f = 0; f < numFrames; ++f) {
        double t = static_cast<double>(f) / static_cast<double>(sampleRate);
        double value = std::sin(2.0 * M_PI * 440.0 * t);

        for (uint16_t ch = 0; ch < channels; ++ch) {
            if (bitsPerSample == 16) {
                int16_t sample = static_cast<int16_t>(value * 32767.0);
                file.write(reinterpret_cast<const char*>(&sample), 2);
            } else if (bitsPerSample == 24) {
                int32_t sample = static_cast<int32_t>(value * 8388607.0);
                file.write(reinterpret_cast<const char*>(&sample), 3);
            }
        }
    }

    file.close();
    return path;
}

// テスト用の不正なファイルを作成
std::string createInvalidFile(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    file.write("NOT_A_WAV_FILE_HEADER", 21);
    file.close();
    return path;
}

// テスト用の最小OGGファイルヘッダを作成
std::string createTestOggFile(const std::string& path,
                               uint8_t channels = 2, uint32_t sampleRate = 44100) {
    std::ofstream file(path, std::ios::binary);

    // OGGページヘッダ
    file.write("OggS", 4);         // capture pattern
    uint8_t version = 0;
    file.write(reinterpret_cast<const char*>(&version), 1);    // version
    uint8_t headerType = 0x02;     // beginning of stream
    file.write(reinterpret_cast<const char*>(&headerType), 1);
    uint64_t granulePos = 0;
    file.write(reinterpret_cast<const char*>(&granulePos), 8);
    uint32_t serialNumber = 1;
    file.write(reinterpret_cast<const char*>(&serialNumber), 4);
    uint32_t pageSequence = 0;
    file.write(reinterpret_cast<const char*>(&pageSequence), 4);
    uint32_t checksum = 0;
    file.write(reinterpret_cast<const char*>(&checksum), 4);
    uint8_t numSegments = 1;
    file.write(reinterpret_cast<const char*>(&numSegments), 1);
    uint8_t segmentSize = 30;      // Vorbis identification header size
    file.write(reinterpret_cast<const char*>(&segmentSize), 1);

    // Vorbis identification header
    uint8_t packetType = 0x01;
    file.write(reinterpret_cast<const char*>(&packetType), 1);
    file.write("vorbis", 6);

    uint32_t vorbisVersion = 0;
    file.write(reinterpret_cast<const char*>(&vorbisVersion), 4);
    file.write(reinterpret_cast<const char*>(&channels), 1);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);

    // 残りのヘッダフィールド（ビットレート等）
    int32_t bitrateMax = 0, bitrateNominal = 128000, bitrateMin = 0;
    file.write(reinterpret_cast<const char*>(&bitrateMax), 4);
    file.write(reinterpret_cast<const char*>(&bitrateNominal), 4);
    file.write(reinterpret_cast<const char*>(&bitrateMin), 4);
    uint8_t blockSizes = 0x88;  // blocksize_0=8, blocksize_1=8
    file.write(reinterpret_cast<const char*>(&blockSizes), 1);
    uint8_t framingFlag = 1;
    file.write(reinterpret_cast<const char*>(&framingFlag), 1);

    file.close();
    return path;
}

} // namespace

class WavDecoderTest : public ::testing::Test {
protected:
    void SetUp() override {
        wavPath_ = "/tmp/ergo_test_440hz.wav";
        createTestWavFile(wavPath_);
    }

    void TearDown() override {
        std::remove(wavPath_.c_str());
    }

    std::string wavPath_;
};

// WAVヘッダが正しく解析されること
TEST_F(WavDecoderTest, ParseHeader) {
    WavDecoder decoder;
    ASSERT_TRUE(decoder.open(wavPath_));

    auto fmt = decoder.getFormat();
    EXPECT_EQ(fmt.sampleRate, 44100u);
    EXPECT_EQ(fmt.channels, 1);
    EXPECT_EQ(fmt.format, SampleFormat::Int16);
    EXPECT_EQ(decoder.getTotalFrames(), 4410u);

    decoder.close();
}

// WAV PCMデータが正しくデコードされること
TEST_F(WavDecoderTest, DecodePcm) {
    WavDecoder decoder;
    ASSERT_TRUE(decoder.open(wavPath_));

    std::vector<float> buffer(4410);
    size_t framesRead = decoder.decode(buffer.data(), 4410);

    EXPECT_EQ(framesRead, 4410u);

    // 最初のサンプルは0付近（440Hz正弦波の開始）
    EXPECT_NEAR(buffer[0], 0.0f, 0.01f);

    // いくつかのサンプルが非ゼロであることを確認
    bool hasNonZero = false;
    for (size_t i = 0; i < framesRead; ++i) {
        if (std::fabs(buffer[i]) > 0.01f) {
            hasNonZero = true;
            break;
        }
    }
    EXPECT_TRUE(hasNonZero);

    decoder.close();
}

// 不正なファイルに対してエラーが返されること
TEST_F(WavDecoderTest, InvalidFile) {
    std::string invalidPath = "/tmp/ergo_test_invalid.wav";
    createInvalidFile(invalidPath);

    WavDecoder decoder;
    EXPECT_FALSE(decoder.open(invalidPath));
    EXPECT_FALSE(decoder.isOpen());

    std::remove(invalidPath.c_str());
}

// 存在しないファイルに対してエラーが返されること
TEST_F(WavDecoderTest, NonexistentFile) {
    WavDecoder decoder;
    EXPECT_FALSE(decoder.open("/tmp/nonexistent_file.wav"));
}

// シークが正しく動作すること
TEST_F(WavDecoderTest, Seek) {
    WavDecoder decoder;
    ASSERT_TRUE(decoder.open(wavPath_));

    // 中間位置にシーク
    ASSERT_TRUE(decoder.seekTo(2205));
    EXPECT_EQ(decoder.getCurrentFrame(), 2205u);

    // そこからデコード
    std::vector<float> buffer(100);
    size_t framesRead = decoder.decode(buffer.data(), 100);
    EXPECT_EQ(framesRead, 100u);

    decoder.close();
}

// 24bit WAVが正しくデコードされること
TEST(WavDecoder24BitTest, Decode24Bit) {
    std::string path = "/tmp/ergo_test_24bit.wav";
    createTestWavFile(path, 44100, 1, 24, 1000);

    WavDecoder decoder;
    ASSERT_TRUE(decoder.open(path));

    auto fmt = decoder.getFormat();
    EXPECT_EQ(fmt.format, SampleFormat::Int24);

    std::vector<float> buffer(1000);
    size_t framesRead = decoder.decode(buffer.data(), 1000);
    EXPECT_EQ(framesRead, 1000u);

    decoder.close();
    std::remove(path.c_str());
}

// ステレオWAVが正しくデコードされること
TEST(WavDecoderStereoTest, DecodeStereo) {
    std::string path = "/tmp/ergo_test_stereo.wav";
    createTestWavFile(path, 44100, 2, 16, 1000);

    WavDecoder decoder;
    ASSERT_TRUE(decoder.open(path));

    auto fmt = decoder.getFormat();
    EXPECT_EQ(fmt.channels, 2);

    std::vector<float> buffer(2000);  // 1000 frames * 2 channels
    size_t framesRead = decoder.decode(buffer.data(), 1000);
    EXPECT_EQ(framesRead, 1000u);

    decoder.close();
    std::remove(path.c_str());
}

// デコーダ種別判定
TEST(DecoderTypeTest, DetectType) {
    EXPECT_EQ(IAudioDecoder::detectType("test.wav"), DecoderType::Wav);
    EXPECT_EQ(IAudioDecoder::detectType("test.WAV"), DecoderType::Wav);
    EXPECT_EQ(IAudioDecoder::detectType("test.wave"), DecoderType::Wav);
    EXPECT_EQ(IAudioDecoder::detectType("test.ogg"), DecoderType::Ogg);
    EXPECT_EQ(IAudioDecoder::detectType("test.mp3"), DecoderType::Unknown);
}

// ファクトリメソッド
TEST(DecoderFactoryTest, CreateDecoder) {
    auto wavDecoder = IAudioDecoder::create("test.wav");
    EXPECT_NE(wavDecoder, nullptr);

    auto oggDecoder = IAudioDecoder::create("test.ogg");
    EXPECT_NE(oggDecoder, nullptr);

    auto unknown = IAudioDecoder::create("test.mp3");
    EXPECT_EQ(unknown, nullptr);
}

// OGGデコーダのヘッダ解析
TEST(OggDecoderTest, ParseHeader) {
    std::string path = "/tmp/ergo_test.ogg";
    createTestOggFile(path, 2, 48000);

    OggDecoder decoder;
    ASSERT_TRUE(decoder.open(path));

    auto fmt = decoder.getFormat();
    EXPECT_EQ(fmt.channels, 2);
    EXPECT_EQ(fmt.sampleRate, 48000u);

    decoder.close();
    std::remove(path.c_str());
}

// 不正なOGGファイル
TEST(OggDecoderTest, InvalidFile) {
    std::string path = "/tmp/ergo_test_invalid.ogg";
    createInvalidFile(path);

    OggDecoder decoder;
    EXPECT_FALSE(decoder.open(path));

    std::remove(path.c_str());
}
