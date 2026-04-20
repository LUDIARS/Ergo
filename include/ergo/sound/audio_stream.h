#pragma once

#include "ergo/sound/types.h"
#include "ergo/sound/audio_decoder.h"
#include <memory>
#include <vector>

namespace ergo::sound {

// =====================================================================
// AudioStream — ファイルストリーミング再生
// =====================================================================
/// デコーダを内部に保持し、ファイルから逐次的にPCMデータを読み出す。
/// 全体をメモリに展開せず、バッファリングしながらストリーム再生を行う。
class AudioStream {
public:
    /// @param bufferFrames 内部バッファのフレーム数（デフォルト4096）
    explicit AudioStream(size_t bufferFrames = 4096);
    ~AudioStream();

    /// ファイルを開いてストリーム準備する
    bool open(const std::string& filePath);

    /// 指定フレーム数だけPCMデータを読み出す（float正規化）
    /// @param output 出力バッファ（frames * channels 分のサイズが必要）
    /// @param frames 読み出すフレーム数
    /// @return 実際に読み出したフレーム数
    size_t read(float* output, size_t frames);

    /// 指定フレーム位置にシークする
    bool seek(size_t framePosition);

    /// ストリームを閉じる
    void close();

    /// ストリーム終端に達したか
    bool isEndOfStream() const;

    /// ファイルが開いているか
    bool isOpen() const;

    /// フォーマット情報を取得する
    AudioFormat getFormat() const;

    /// 総フレーム数を取得する
    size_t getTotalFrames() const;

    /// 現在の再生フレーム位置を取得する
    size_t getCurrentFrame() const;

private:
    std::unique_ptr<IAudioDecoder>  decoder_;
    std::vector<float>              buffer_;
    size_t                          bufferFrames_;
    size_t                          bufferReadPos_  = 0;
    size_t                          bufferValidFrames_ = 0;
    bool                            eos_            = false;

    /// 内部バッファを補充する
    size_t fillBuffer();
};

} // namespace ergo::sound
