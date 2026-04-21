// AudioDecoder — WAVファイルデコーダ実装

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
    return DecoderType::Unknown;
}

std::unique_ptr<IAudioDecoder> IAudioDecoder::create(const std::string& filePath) {
    switch (detectType(filePath)) {
        case DecoderType::Wav: return std::make_unique<WavDecoder>();
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

} // namespace ergo::sound
