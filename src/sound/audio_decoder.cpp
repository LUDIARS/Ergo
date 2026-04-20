// AudioDecoder — WAV/OGGファイルデコーダ実装

#include "ergo/sound/audio_decoder.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace ergo::sound {

// =====================================================================
// IAudioDecoder — 静的メソッド
// =====================================================================

DecoderType IAudioDecoder::detectType(const std::string& filePath) {
    auto ext = filePath.substr(filePath.find_last_of('.') + 1);
    std::string lower;
    lower.reserve(ext.size());
    for (char c : ext) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    if (lower == "wav" || lower == "wave") return DecoderType::Wav;
    if (lower == "ogg")                    return DecoderType::Ogg;
    return DecoderType::Unknown;
}

std::unique_ptr<IAudioDecoder> IAudioDecoder::create(const std::string& filePath) {
    switch (detectType(filePath)) {
        case DecoderType::Wav: return std::make_unique<WavDecoder>();
        case DecoderType::Ogg: return std::make_unique<OggDecoder>();
        default: return nullptr;
    }
}

// =====================================================================
// WavDecoder
// =====================================================================

// WAVヘッダ構造体（パック）
#pragma pack(push, 1)
struct RiffHeader {
    char        chunkId[4];     // "RIFF"
    uint32_t    chunkSize;
    char        format[4];      // "WAVE"
};

struct WavSubchunk {
    char        subchunkId[4];
    uint32_t    subchunkSize;
};

struct WavFmtData {
    uint16_t    audioFormat;    // 1 = PCM, 3 = IEEE float
    uint16_t    numChannels;
    uint32_t    sampleRate;
    uint32_t    byteRate;
    uint16_t    blockAlign;
    uint16_t    bitsPerSample;
};
#pragma pack(pop)

WavDecoder::~WavDecoder() {
    close();
}

bool WavDecoder::open(const std::string& filePath) {
    close();

    stream_.open(filePath, std::ios::binary);
    if (!stream_.is_open()) return false;

    if (!parseHeader()) {
        close();
        return false;
    }

    opened_ = true;
    currentFrame_ = 0;
    return true;
}

bool WavDecoder::parseHeader() {
    RiffHeader riff{};
    stream_.read(reinterpret_cast<char*>(&riff), sizeof(riff));
    if (!stream_ || std::memcmp(riff.chunkId, "RIFF", 4) != 0 ||
        std::memcmp(riff.format, "WAVE", 4) != 0) {
        return false;
    }

    bool foundFmt = false;
    bool foundData = false;
    WavFmtData fmtData{};

    while (stream_ && !foundData) {
        WavSubchunk sub{};
        stream_.read(reinterpret_cast<char*>(&sub), sizeof(sub));
        if (!stream_) break;

        if (std::memcmp(sub.subchunkId, "fmt ", 4) == 0) {
            if (sub.subchunkSize < sizeof(WavFmtData)) return false;
            stream_.read(reinterpret_cast<char*>(&fmtData), sizeof(fmtData));
            if (!stream_) return false;
            // 余分なfmtデータをスキップ
            if (sub.subchunkSize > sizeof(WavFmtData)) {
                stream_.seekg(sub.subchunkSize - sizeof(WavFmtData), std::ios::cur);
            }
            foundFmt = true;
        } else if (std::memcmp(sub.subchunkId, "data", 4) == 0) {
            dataOffset_ = static_cast<size_t>(stream_.tellg());
            uint32_t bytesPerFrame = fmtData.numChannels * (fmtData.bitsPerSample / 8);
            if (bytesPerFrame == 0) return false;
            totalFrames_ = sub.subchunkSize / bytesPerFrame;
            foundData = true;
        } else {
            // 未知のチャンクをスキップ
            stream_.seekg(sub.subchunkSize, std::ios::cur);
        }
    }

    if (!foundFmt || !foundData) return false;

    // サポートされるフォーマットのみ
    if (fmtData.audioFormat != 1 && fmtData.audioFormat != 3) return false;

    format_.sampleRate = fmtData.sampleRate;
    format_.channels = fmtData.numChannels;

    if (fmtData.audioFormat == 3) {
        format_.format = SampleFormat::Float32;
    } else {
        switch (fmtData.bitsPerSample) {
            case 16: format_.format = SampleFormat::Int16;  break;
            case 24: format_.format = SampleFormat::Int24;  break;
            case 32: format_.format = SampleFormat::Float32; break;
            default: return false;
        }
    }

    return true;
}

size_t WavDecoder::decode(float* buffer, size_t maxFrames) {
    if (!opened_ || !stream_) return 0;

    size_t framesToRead = std::min(maxFrames, totalFrames_ - currentFrame_);
    if (framesToRead == 0) return 0;

    size_t bytesPerFrame = format_.bytesPerFrame();
    size_t bytesToRead = framesToRead * bytesPerFrame;
    std::vector<uint8_t> rawData(bytesToRead);

    stream_.read(reinterpret_cast<char*>(rawData.data()), static_cast<std::streamsize>(bytesToRead));
    size_t bytesRead = static_cast<size_t>(stream_.gcount());
    size_t framesRead = bytesRead / bytesPerFrame;

    // floatに変換
    size_t sampleCount = framesRead * format_.channels;
    switch (format_.format) {
        case SampleFormat::Int16: {
            const auto* src = reinterpret_cast<const int16_t*>(rawData.data());
            for (size_t i = 0; i < sampleCount; ++i) {
                buffer[i] = static_cast<float>(src[i]) / 32768.0f;
            }
            break;
        }
        case SampleFormat::Int24: {
            for (size_t i = 0; i < sampleCount; ++i) {
                size_t offset = i * 3;
                int32_t sample = static_cast<int32_t>(rawData[offset])
                               | (static_cast<int32_t>(rawData[offset + 1]) << 8)
                               | (static_cast<int32_t>(rawData[offset + 2]) << 16);
                // 符号拡張
                if (sample & 0x800000) sample |= 0xFF000000;
                buffer[i] = static_cast<float>(sample) / 8388608.0f;
            }
            break;
        }
        case SampleFormat::Float32: {
            std::memcpy(buffer, rawData.data(), sampleCount * sizeof(float));
            break;
        }
    }

    currentFrame_ += framesRead;
    return framesRead;
}

bool WavDecoder::seekTo(size_t framePosition) {
    if (!opened_) return false;

    size_t targetFrame = std::min(framePosition, totalFrames_);
    size_t byteOffset = dataOffset_ + targetFrame * format_.bytesPerFrame();

    stream_.clear();
    stream_.seekg(static_cast<std::streamoff>(byteOffset), std::ios::beg);
    if (!stream_) return false;

    currentFrame_ = targetFrame;
    return true;
}

AudioFormat WavDecoder::getFormat() const { return format_; }
size_t WavDecoder::getTotalFrames() const { return totalFrames_; }
size_t WavDecoder::getCurrentFrame() const { return currentFrame_; }
bool WavDecoder::isOpen() const { return opened_; }

void WavDecoder::close() {
    if (stream_.is_open()) stream_.close();
    opened_ = false;
    currentFrame_ = 0;
    totalFrames_ = 0;
    dataOffset_ = 0;
}

// =====================================================================
// OggDecoder — 簡易OGG Vorbisデコーダ
// =====================================================================
// 注: 完全なVorbisデコーダの実装は非常に大規模なため、
// ここではOGGコンテナの解析とヘッダ読み取りを行い、
// PCMデータの読み取り基盤を提供する。
// 本番環境では stb_vorbis や libvorbis との連携を想定する。

OggDecoder::~OggDecoder() {
    close();
}

bool OggDecoder::open(const std::string& filePath) {
    close();

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    fileData_.resize(fileSize);
    file.read(reinterpret_cast<char*>(fileData_.data()), static_cast<std::streamsize>(fileSize));
    file.close();

    if (!parseAndDecode()) {
        close();
        return false;
    }

    opened_ = true;
    currentFrame_ = 0;
    return true;
}

bool OggDecoder::parseAndDecode() {
    // OGGマジックナンバーの確認: "OggS"
    if (fileData_.size() < 27) return false;
    if (fileData_[0] != 'O' || fileData_[1] != 'g' ||
        fileData_[2] != 'g' || fileData_[3] != 'S') {
        return false;
    }

    // Vorbisヘッダの解析（最小限）
    // Vorbis identification header は OGGページ内に格納される
    // ページヘッダ: 27バイト + セグメントテーブル
    uint8_t numSegments = fileData_[26];
    size_t headerSize = 27 + numSegments;
    if (fileData_.size() < headerSize + 7) return false;

    // セグメントデータ開始位置
    size_t segmentStart = headerSize;

    // Vorbis identification header: パケットタイプ(1) + "vorbis"(6)
    if (fileData_[segmentStart] != 0x01) return false;
    if (fileData_[segmentStart + 1] != 'v' || fileData_[segmentStart + 2] != 'o' ||
        fileData_[segmentStart + 3] != 'r' || fileData_[segmentStart + 4] != 'b' ||
        fileData_[segmentStart + 5] != 'i' || fileData_[segmentStart + 6] != 's') {
        return false;
    }

    // Vorbis identification headerの解析
    if (fileData_.size() < segmentStart + 30) return false;

    // vorbis_version (4bytes) at offset 7
    // channels (1byte) at offset 11
    // sample_rate (4bytes) at offset 12
    uint8_t channels = fileData_[segmentStart + 11];
    uint32_t sampleRate = 0;
    std::memcpy(&sampleRate, &fileData_[segmentStart + 12], 4);

    if (channels == 0 || sampleRate == 0) return false;

    format_.channels = channels;
    format_.sampleRate = sampleRate;
    format_.format = SampleFormat::Float32;

    // 注: 実際のVorbisデコードは外部ライブラリに委譲する設計
    // ここではヘッダ解析のみ行い、PCMデータは空として扱う
    totalFrames_ = 0;
    decodedPcm_.clear();

    return true;
}

size_t OggDecoder::decode(float* buffer, size_t maxFrames) {
    if (!opened_) return 0;

    size_t available = (totalFrames_ > currentFrame_) ? totalFrames_ - currentFrame_ : 0;
    size_t framesToRead = std::min(maxFrames, available);
    if (framesToRead == 0) return 0;

    size_t sampleOffset = currentFrame_ * format_.channels;
    size_t sampleCount = framesToRead * format_.channels;
    std::memcpy(buffer, decodedPcm_.data() + sampleOffset, sampleCount * sizeof(float));

    currentFrame_ += framesToRead;
    return framesToRead;
}

bool OggDecoder::seekTo(size_t framePosition) {
    if (!opened_) return false;
    currentFrame_ = std::min(framePosition, totalFrames_);
    return true;
}

AudioFormat OggDecoder::getFormat() const { return format_; }
size_t OggDecoder::getTotalFrames() const { return totalFrames_; }
size_t OggDecoder::getCurrentFrame() const { return currentFrame_; }
bool OggDecoder::isOpen() const { return opened_; }

void OggDecoder::close() {
    fileData_.clear();
    decodedPcm_.clear();
    opened_ = false;
    currentFrame_ = 0;
    totalFrames_ = 0;
}

} // namespace ergo::sound
