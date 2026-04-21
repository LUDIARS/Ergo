#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>

namespace ergo::sound {

// =====================================================================
// サンプルフォーマット
// =====================================================================
enum class SampleFormat : uint8_t {
    Int16,      // 16bit整数 PCM
    Int24,      // 24bit整数 PCM
    Float32     // 32bit浮動小数点
};

// =====================================================================
// オーディオフォーマット
// =====================================================================
struct AudioFormat {
    uint32_t    sampleRate  = 44100;
    uint16_t    channels    = 2;
    SampleFormat format     = SampleFormat::Int16;

    /// 1サンプルあたりのバイト数（1チャンネル分）
    uint32_t bytesPerSample() const {
        switch (format) {
            case SampleFormat::Int16:   return 2;
            case SampleFormat::Int24:   return 3;
            case SampleFormat::Float32: return 4;
        }
        return 2;
    }

    /// 1フレームあたりのバイト数（全チャンネル分）
    uint32_t bytesPerFrame() const {
        return bytesPerSample() * channels;
    }
};

// =====================================================================
// ボイス状態
// =====================================================================
enum class VoiceState : uint8_t {
    Stopped,
    Playing,
    Paused
};

// =====================================================================
// デコーダ種別
// =====================================================================
enum class DecoderType : uint8_t {
    Unknown,
    Wav,
    Ogg
};

// =====================================================================
// クォンタイズグリッドサイズ
// =====================================================================
enum class GridSize : uint8_t {
    Whole       = 1,    // 全音符
    Half        = 2,    // 2分音符
    Quarter     = 4,    // 4分音符
    Eighth      = 8,    // 8分音符
    Sixteenth   = 16,   // 16分音符
    ThirtySecond = 32   // 32分音符
};

// =====================================================================
// オーディオバッファ
// =====================================================================
struct AudioBuffer {
    std::vector<float>  data;       // インターリーブされたPCMデータ（float正規化）
    AudioFormat         format;
    size_t              frameCount = 0;

    /// バッファのクリア
    void clear() {
        data.clear();
        frameCount = 0;
    }

    /// フレーム数に合わせてリサイズ
    void resize(size_t frames) {
        frameCount = frames;
        data.resize(frames * format.channels);
    }
};

// =====================================================================
// ボイスハンドル
// =====================================================================
using VoiceId = uint32_t;
constexpr VoiceId InvalidVoiceId = 0;

// =====================================================================
// オーディオエンジン設定
// =====================================================================
struct AudioEngineConfig {
    uint32_t    sampleRate      = 44100;
    uint16_t    channels        = 2;
    uint32_t    bufferFrames    = 512;      // コールバックあたりのフレーム数
    uint32_t    maxVoices       = 32;       // 最大同時再生数
    int         threadPriority  = 90;       // スレッド優先度（0-99、高い方が優先）
};

// =====================================================================
// クォンタイズ設定
// =====================================================================
struct QuantizeConfig {
    double      bpm         = 120.0;
    uint32_t    sampleRate  = 44100;
    GridSize    gridSize    = GridSize::Quarter;
};

// =====================================================================
// フェード種別
// =====================================================================
enum class FadeType : uint8_t {
    Linear,
    EaseIn,
    EaseOut
};

// =====================================================================
// エンベロープポイント
// =====================================================================
struct EnvelopePoint {
    size_t  framePosition;  // フレーム位置
    float   amplitude;      // 振幅（0.0〜1.0）
};

// =====================================================================
// コールバック型
// =====================================================================
using AudioCallback = std::function<void(float* outputBuffer, size_t frameCount, uint16_t channels)>;

} // namespace ergo::sound
