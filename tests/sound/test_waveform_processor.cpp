// WaveformProcessor テスト

#include <gtest/gtest.h>
#include "ergo/sound/waveform_processor.h"
#include <cmath>

using namespace ergo::sound;

namespace {

// テスト用バッファを生成（440Hz正弦波）
AudioBuffer createSineBuffer(size_t frameCount, uint16_t channels = 1,
                              uint32_t sampleRate = 44100, float frequency = 440.0f) {
    AudioBuffer buf;
    buf.format.sampleRate = sampleRate;
    buf.format.channels = channels;
    buf.format.format = SampleFormat::Float32;
    buf.resize(frameCount);

    for (size_t f = 0; f < frameCount; ++f) {
        float t = static_cast<float>(f) / static_cast<float>(sampleRate);
        float value = std::sin(2.0f * static_cast<float>(M_PI) * frequency * t);
        for (uint16_t ch = 0; ch < channels; ++ch) {
            buf.data[f * channels + ch] = value;
        }
    }

    return buf;
}

// バッファの最大絶対値を取得
float getMaxAbs(const AudioBuffer& buf) {
    float maxAbs = 0.0f;
    for (float s : buf.data) {
        float abs = std::fabs(s);
        if (abs > maxAbs) maxAbs = abs;
    }
    return maxAbs;
}

} // namespace

// ゲイン適用で振幅が正しく変化すること
TEST(WaveformProcessorTest, Amplify) {
    auto buf = createSineBuffer(1000);
    float originalMax = getMaxAbs(buf);

    WaveformProcessor::amplify(buf, 0.5f);

    float newMax = getMaxAbs(buf);
    EXPECT_NEAR(newMax, originalMax * 0.5f, 0.001f);
}

TEST(WaveformProcessorTest, AmplifyDouble) {
    auto buf = createSineBuffer(1000);

    WaveformProcessor::amplify(buf, 2.0f);

    float maxVal = getMaxAbs(buf);
    EXPECT_NEAR(maxVal, 2.0f, 0.01f);
}

// ノーマライズで最大振幅が1.0に正規化されること
TEST(WaveformProcessorTest, Normalize) {
    auto buf = createSineBuffer(1000);
    WaveformProcessor::amplify(buf, 0.3f);

    WaveformProcessor::normalize(buf);

    float maxVal = getMaxAbs(buf);
    EXPECT_NEAR(maxVal, 1.0f, 0.001f);
}

TEST(WaveformProcessorTest, NormalizeEmpty) {
    AudioBuffer buf;
    buf.format.channels = 1;
    buf.resize(0);

    // 空バッファでクラッシュしないこと
    WaveformProcessor::normalize(buf);
    EXPECT_EQ(buf.frameCount, 0u);
}

// フェードインが正しく適用されること
TEST(WaveformProcessorTest, FadeIn) {
    auto buf = createSineBuffer(1000);

    WaveformProcessor::fadeIn(buf, 500);

    // 先頭は0に近い
    EXPECT_NEAR(buf.data[0], 0.0f, 0.001f);

    // フェード後は元の値に近い
    auto original = createSineBuffer(1000);
    EXPECT_NEAR(buf.data[999], original.data[999], 0.001f);
}

// フェードアウトが正しく適用されること
TEST(WaveformProcessorTest, FadeOut) {
    auto buf = createSineBuffer(1000);

    WaveformProcessor::fadeOut(buf, 500);

    // 末尾は0に近い
    EXPECT_NEAR(buf.data[999], 0.0f, 0.001f);

    // フェード前は元の値に近い
    auto original = createSineBuffer(1000);
    EXPECT_NEAR(buf.data[0], original.data[0], 0.001f);
}

// EaseInフェード
TEST(WaveformProcessorTest, FadeInEaseIn) {
    auto buf = createSineBuffer(1000);
    WaveformProcessor::fadeIn(buf, 500, FadeType::EaseIn);

    // 先頭は0
    EXPECT_NEAR(buf.data[0], 0.0f, 0.001f);
}

// ピッチシフトが正しく動作すること
TEST(WaveformProcessorTest, PitchShiftUp) {
    auto buf = createSineBuffer(4410);  // 0.1秒

    auto result = WaveformProcessor::pitchShift(buf, 12.0f);  // 1オクターブ上

    // ピッチを上げると長さが短くなる
    EXPECT_LT(result.frameCount, buf.frameCount);
    // 1オクターブ上 = 半分の長さ
    EXPECT_NEAR(static_cast<double>(result.frameCount),
                static_cast<double>(buf.frameCount) / 2.0, 2.0);
}

TEST(WaveformProcessorTest, PitchShiftDown) {
    auto buf = createSineBuffer(4410);

    auto result = WaveformProcessor::pitchShift(buf, -12.0f);  // 1オクターブ下

    // ピッチを下げると長さが長くなる
    EXPECT_GT(result.frameCount, buf.frameCount);
}

// 2つの波形が正しくミキシングされること
TEST(WaveformProcessorTest, Mix) {
    auto a = createSineBuffer(1000, 1, 44100, 440.0f);
    auto b = createSineBuffer(1000, 1, 44100, 880.0f);

    auto result = WaveformProcessor::mix(a, b, 0.5f);

    EXPECT_EQ(result.frameCount, 1000u);

    // 50/50ミックスなので各サンプルは (a + b) / 2 に近い
    for (size_t i = 0; i < 100; ++i) {
        float expected = a.data[i] * 0.5f + b.data[i] * 0.5f;
        EXPECT_NEAR(result.data[i], expected, 0.001f);
    }
}

// 異なる長さのバッファのミキシング
TEST(WaveformProcessorTest, MixDifferentLengths) {
    auto a = createSineBuffer(1000);
    auto b = createSineBuffer(500);

    auto result = WaveformProcessor::mix(a, b, 0.5f);
    EXPECT_EQ(result.frameCount, 1000u);
}

// エンベロープ適用
TEST(WaveformProcessorTest, ApplyEnvelope) {
    auto buf = createSineBuffer(1000);

    std::vector<EnvelopePoint> env = {
        {0,   0.0f},
        {500, 1.0f},
        {999, 0.0f}
    };

    WaveformProcessor::applyEnvelope(buf, env);

    // 先頭は0に近い
    EXPECT_NEAR(buf.data[0], 0.0f, 0.001f);
    // 末尾も0に近い
    EXPECT_NEAR(buf.data[999], 0.0f, 0.01f);
}

// DCオフセット除去
TEST(WaveformProcessorTest, RemoveDCOffset) {
    auto buf = createSineBuffer(1000);
    // DCオフセットを追加
    for (auto& s : buf.data) s += 0.5f;

    WaveformProcessor::removeDCOffset(buf);

    // 平均が0に近い
    double sum = 0.0;
    for (float s : buf.data) sum += s;
    double avg = sum / static_cast<double>(buf.data.size());
    EXPECT_NEAR(avg, 0.0, 0.001);
}

// リバース
TEST(WaveformProcessorTest, Reverse) {
    auto buf = createSineBuffer(100);
    auto original = buf;

    WaveformProcessor::reverse(buf);

    // 最初のサンプルが元の最後のサンプルと一致
    EXPECT_NEAR(buf.data[0], original.data[99], 0.001f);
    EXPECT_NEAR(buf.data[99], original.data[0], 0.001f);
}
