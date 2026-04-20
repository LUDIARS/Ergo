#pragma once

#include "ergo/sound/types.h"

namespace ergo::sound {

// =====================================================================
// Quantizer — クォンタイズシステム
// =====================================================================
/// BPMとグリッドサイズに基づき、サンプル位置を音楽的なグリッドにスナップする。
class Quantizer {
public:
    /// デフォルトコンストラクタ（120BPM, 44100Hz, 4分音符）
    Quantizer();

    /// 設定付きコンストラクタ
    explicit Quantizer(const QuantizeConfig& config);

    /// BPMを設定する
    void setBPM(double bpm);

    /// BPMを取得する
    double getBPM() const;

    /// サンプルレートを設定する
    void setSampleRate(uint32_t sampleRate);

    /// グリッドサイズを設定する
    void setGridSize(GridSize gridSize);

    /// グリッドサイズを取得する
    GridSize getGridSize() const;

    /// サンプル位置を最寄りのグリッドにスナップする
    size_t quantize(size_t samplePosition) const;

    /// 指定位置以降の次のグリッドポイントを取得する
    size_t getNextGridPoint(size_t samplePosition) const;

    /// 指定位置以前の前のグリッドポイントを取得する
    size_t getPreviousGridPoint(size_t samplePosition) const;

    /// 1グリッドあたりのサンプル数を取得する
    double getSamplesPerGrid() const;

    /// 1拍（4分音符）あたりのサンプル数を取得する
    double getSamplesPerBeat() const;

    /// サンプル位置をビート数に変換する
    double samplesToBeats(size_t samples) const;

    /// ビート数をサンプル位置に変換する
    size_t beatsToSamples(double beats) const;

    /// サンプル位置を小節数に変換する（4/4拍子前提）
    double samplesToBars(size_t samples) const;

    /// 小節数をサンプル位置に変換する（4/4拍子前提）
    size_t barsToSamples(double bars) const;

private:
    QuantizeConfig config_;

    /// 内部キャッシュの再計算
    void recalculate();

    double samplesPerBeat_  = 0.0;
    double samplesPerGrid_  = 0.0;
};

} // namespace ergo::sound
