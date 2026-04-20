#pragma once

#include "ergo/sound/types.h"
#include <vector>

namespace ergo::sound {

// =====================================================================
// WaveformProcessor — 波形操作プロセッサ
// =====================================================================
/// AudioBufferに対して各種波形操作を行う静的ユーティリティクラス。
class WaveformProcessor {
public:
    WaveformProcessor() = delete;

    /// ゲイン（振幅）を適用する
    /// @param buffer 対象バッファ
    /// @param gain ゲイン値（1.0 = 変化なし）
    static void amplify(AudioBuffer& buffer, float gain);

    /// ノーマライズ（最大振幅を1.0に正規化）
    static void normalize(AudioBuffer& buffer);

    /// フェードインを適用する
    /// @param buffer 対象バッファ
    /// @param durationFrames フェード期間（フレーム数）
    /// @param type フェードカーブ種別
    static void fadeIn(AudioBuffer& buffer, size_t durationFrames, FadeType type = FadeType::Linear);

    /// フェードアウトを適用する
    /// @param buffer 対象バッファ
    /// @param durationFrames フェード期間（フレーム数）
    /// @param type フェードカーブ種別
    static void fadeOut(AudioBuffer& buffer, size_t durationFrames, FadeType type = FadeType::Linear);

    /// ピッチシフト（リサンプリングベース）
    /// @param buffer 対象バッファ
    /// @param semitones 半音単位のシフト量（正=上げ、負=下げ）
    /// @return ピッチシフト後の新しいバッファ
    static AudioBuffer pitchShift(const AudioBuffer& buffer, float semitones);

    /// 2つのバッファをミキシングする
    /// @param a バッファA
    /// @param b バッファB
    /// @param ratioA バッファAの混合比率（0.0〜1.0）
    /// @return ミキシング後の新しいバッファ
    static AudioBuffer mix(const AudioBuffer& a, const AudioBuffer& b, float ratioA = 0.5f);

    /// エンベロープを適用する
    /// @param buffer 対象バッファ
    /// @param points エンベロープポイント（フレーム位置と振幅のペア、昇順）
    static void applyEnvelope(AudioBuffer& buffer, const std::vector<EnvelopePoint>& points);

    /// DCオフセットを除去する
    static void removeDCOffset(AudioBuffer& buffer);

    /// バッファを反転する（リバース）
    static void reverse(AudioBuffer& buffer);

private:
    /// フェードカーブ値を計算する
    static float calculateFadeCurve(float t, FadeType type);
};

} // namespace ergo::sound
