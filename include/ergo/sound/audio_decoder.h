#pragma once

#include "ergo/sound/types.h"
#include <memory>
#include <fstream>

namespace ergo::sound {

// =====================================================================
// IAudioDecoder — オーディオデコーダインタフェース
// =====================================================================
class IAudioDecoder {
public:
    virtual ~IAudioDecoder() = default;

    /// ファイルを開く
    virtual bool open(const std::string& filePath) = 0;

    /// PCMデータをデコードする（float正規化）
    /// @param buffer 出力バッファ
    /// @param maxFrames 最大フレーム数
    /// @return デコードされたフレーム数
    virtual size_t decode(float* buffer, size_t maxFrames) = 0;

    /// 指定フレーム位置にシークする
    virtual bool seekTo(size_t framePosition) = 0;

    /// フォーマット情報を取得する
    virtual AudioFormat getFormat() const = 0;

    /// 総フレーム数を取得する
    virtual size_t getTotalFrames() const = 0;

    /// 現在のフレーム位置を取得する
    virtual size_t getCurrentFrame() const = 0;

    /// ファイルを閉じる
    virtual void close() = 0;

    /// ファイルが開いているか
    virtual bool isOpen() const = 0;

    /// ファイルパスからデコーダ種別を判定する
    static DecoderType detectType(const std::string& filePath);

    /// ファイルパスに応じたデコーダを生成する
    static std::unique_ptr<IAudioDecoder> create(const std::string& filePath);
};

// =====================================================================
// WavDecoder — WAVファイルデコーダ
// =====================================================================
class WavDecoder : public IAudioDecoder {
public:
    WavDecoder() = default;
    ~WavDecoder() override;

    bool open(const std::string& filePath) override;
    size_t decode(float* buffer, size_t maxFrames) override;
    bool seekTo(size_t framePosition) override;
    AudioFormat getFormat() const override;
    size_t getTotalFrames() const override;
    size_t getCurrentFrame() const override;
    void close() override;
    bool isOpen() const override;

private:
    std::ifstream   stream_;
    AudioFormat     format_;
    size_t          totalFrames_    = 0;
    size_t          currentFrame_   = 0;
    size_t          dataOffset_     = 0;    // PCMデータ開始位置
    bool            opened_         = false;

    bool parseHeader();
};

// =====================================================================
// OggDecoder — OGG Vorbisデコーダ（簡易実装）
// =====================================================================
class OggDecoder : public IAudioDecoder {
public:
    OggDecoder() = default;
    ~OggDecoder() override;

    bool open(const std::string& filePath) override;
    size_t decode(float* buffer, size_t maxFrames) override;
    bool seekTo(size_t framePosition) override;
    AudioFormat getFormat() const override;
    size_t getTotalFrames() const override;
    size_t getCurrentFrame() const override;
    void close() override;
    bool isOpen() const override;

private:
    std::vector<uint8_t>    fileData_;
    AudioFormat             format_;
    size_t                  totalFrames_    = 0;
    size_t                  currentFrame_   = 0;
    bool                    opened_         = false;

    // Vorbis デコード用の内部状態
    std::vector<float>      decodedPcm_;    // デコード済みPCM全体
    bool parseAndDecode();
};

} // namespace ergo::sound
