// Quantizer テスト

#include <gtest/gtest.h>
#include "ergo/sound/quantizer.h"
#include <cmath>

using namespace ergo::sound;

// デフォルト設定（120BPM, 44100Hz, Quarter）
TEST(QuantizerTest, DefaultConfig) {
    Quantizer q;
    EXPECT_DOUBLE_EQ(q.getBPM(), 120.0);
    EXPECT_EQ(q.getGridSize(), GridSize::Quarter);
}

// BPMとサンプルレートに基づくビート・サンプル変換が正しいこと
TEST(QuantizerTest, SamplesPerBeat) {
    Quantizer q;
    // 120BPM, 44100Hz: 1拍 = 44100 * 60 / 120 = 22050 samples
    EXPECT_DOUBLE_EQ(q.getSamplesPerBeat(), 22050.0);
}

TEST(QuantizerTest, BeatsToSamplesAndBack) {
    Quantizer q;

    size_t samples = q.beatsToSamples(4.0);  // 4拍 = 88200 samples
    EXPECT_EQ(samples, 88200u);

    double beats = q.samplesToBeats(88200);
    EXPECT_DOUBLE_EQ(beats, 4.0);
}

// 小節変換
TEST(QuantizerTest, BarsConversion) {
    Quantizer q;

    // 1小節 = 4拍 = 88200 samples
    size_t samples = q.barsToSamples(1.0);
    EXPECT_EQ(samples, 88200u);

    double bars = q.samplesToBars(88200);
    EXPECT_DOUBLE_EQ(bars, 1.0);
}

// グリッドサイズによるサンプル数の変化
TEST(QuantizerTest, GridSizeSamples) {
    Quantizer q;

    q.setGridSize(GridSize::Quarter);
    double quarterSamples = q.getSamplesPerGrid();
    EXPECT_DOUBLE_EQ(quarterSamples, 22050.0);  // 1拍

    q.setGridSize(GridSize::Eighth);
    double eighthSamples = q.getSamplesPerGrid();
    EXPECT_DOUBLE_EQ(eighthSamples, 11025.0);   // 0.5拍

    q.setGridSize(GridSize::Sixteenth);
    double sixteenthSamples = q.getSamplesPerGrid();
    EXPECT_DOUBLE_EQ(sixteenthSamples, 5512.5);  // 0.25拍

    q.setGridSize(GridSize::Half);
    double halfSamples = q.getSamplesPerGrid();
    EXPECT_DOUBLE_EQ(halfSamples, 44100.0);  // 2拍

    q.setGridSize(GridSize::Whole);
    double wholeSamples = q.getSamplesPerGrid();
    EXPECT_DOUBLE_EQ(wholeSamples, 88200.0);  // 4拍
}

// クォンタイズでサンプル位置が最寄りのグリッドにスナップされること
TEST(QuantizerTest, QuantizeSnap) {
    Quantizer q;  // 120BPM, Quarter grid = 22050 samples

    // ちょうどグリッド上
    EXPECT_EQ(q.quantize(0), 0u);
    EXPECT_EQ(q.quantize(22050), 22050u);
    EXPECT_EQ(q.quantize(44100), 44100u);

    // グリッドの前半（前のグリッドにスナップ）
    EXPECT_EQ(q.quantize(5000), 0u);

    // グリッドの後半（次のグリッドにスナップ）
    EXPECT_EQ(q.quantize(17000), 22050u);

    // ちょうど中間
    size_t mid = 11025;
    size_t quantized = q.quantize(mid);
    // roundの仕様で11025は22050にスナップ
    EXPECT_TRUE(quantized == 0u || quantized == 22050u);
}

// 次のグリッドポイント
TEST(QuantizerTest, NextGridPoint) {
    Quantizer q;

    EXPECT_EQ(q.getNextGridPoint(0), 22050u);
    EXPECT_EQ(q.getNextGridPoint(100), 22050u);
    EXPECT_EQ(q.getNextGridPoint(22050), 44100u);
}

// 前のグリッドポイント
TEST(QuantizerTest, PreviousGridPoint) {
    Quantizer q;

    EXPECT_EQ(q.getPreviousGridPoint(22050), 0u);
    EXPECT_EQ(q.getPreviousGridPoint(30000), 22050u);
    EXPECT_EQ(q.getPreviousGridPoint(0), 0u);  // 0以前はない
}

// BPM変更
TEST(QuantizerTest, ChangeBPM) {
    Quantizer q;

    q.setBPM(60.0);
    // 60BPM: 1拍 = 44100 samples
    EXPECT_DOUBLE_EQ(q.getSamplesPerBeat(), 44100.0);
    EXPECT_EQ(q.beatsToSamples(1.0), 44100u);

    q.setBPM(180.0);
    // 180BPM: 1拍 = 44100 * 60 / 180 = 14700 samples
    EXPECT_DOUBLE_EQ(q.getSamplesPerBeat(), 14700.0);
}

// 設定付きコンストラクタ
TEST(QuantizerTest, ConfigConstructor) {
    QuantizeConfig config;
    config.bpm = 140.0;
    config.sampleRate = 48000;
    config.gridSize = GridSize::Eighth;

    Quantizer q(config);

    EXPECT_DOUBLE_EQ(q.getBPM(), 140.0);
    EXPECT_EQ(q.getGridSize(), GridSize::Eighth);

    // 140BPM, 48000Hz: 1拍 = 48000 * 60 / 140
    double expectedSamplesPerBeat = 48000.0 * 60.0 / 140.0;
    EXPECT_DOUBLE_EQ(q.getSamplesPerBeat(), expectedSamplesPerBeat);
}

// 8分音符グリッドでのクォンタイズ
TEST(QuantizerTest, EighthNoteQuantize) {
    Quantizer q;
    q.setGridSize(GridSize::Eighth);
    // 8分音符 = 11025 samples

    EXPECT_EQ(q.quantize(0), 0u);
    EXPECT_EQ(q.quantize(11025), 11025u);
    EXPECT_EQ(q.quantize(3000), 0u);
    EXPECT_EQ(q.quantize(9000), 11025u);
}

// 16分音符グリッドでのクォンタイズ
TEST(QuantizerTest, SixteenthNoteQuantize) {
    Quantizer q;
    q.setGridSize(GridSize::Sixteenth);
    // 16分音符 = 5512.5 samples

    size_t pos = q.quantize(2000);
    EXPECT_EQ(pos, 0u);

    pos = q.quantize(5000);
    EXPECT_EQ(pos, static_cast<size_t>(5512.5));
}
