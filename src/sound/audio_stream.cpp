// AudioStream — ファイルストリーミング再生実装

#include "ergo/sound/audio_stream.h"
#include <algorithm>
#include <cstring>

namespace ergo::sound {

AudioStream::AudioStream(size_t bufferFrames)
    : bufferFrames_(bufferFrames) {
}

AudioStream::~AudioStream() {
    close();
}

bool AudioStream::open(const std::string& filePath) {
    close();

    decoder_ = IAudioDecoder::create(filePath);
    if (!decoder_) return false;

    if (!decoder_->open(filePath)) {
        decoder_.reset();
        return false;
    }

    // 内部バッファをフォーマットに合わせて確保
    auto fmt = decoder_->getFormat();
    buffer_.resize(bufferFrames_ * fmt.channels);
    bufferReadPos_ = 0;
    bufferValidFrames_ = 0;
    eos_ = false;

    return true;
}

size_t AudioStream::read(float* output, size_t frames) {
    if (!decoder_ || !decoder_->isOpen()) return 0;

    size_t totalRead = 0;
    auto fmt = decoder_->getFormat();

    while (totalRead < frames && !eos_) {
        // バッファに残りがあればそこから読み出す
        if (bufferReadPos_ < bufferValidFrames_) {
            size_t available = bufferValidFrames_ - bufferReadPos_;
            size_t toRead = std::min(available, frames - totalRead);
            size_t sampleOffset = bufferReadPos_ * fmt.channels;
            size_t sampleCount = toRead * fmt.channels;

            std::memcpy(output + totalRead * fmt.channels,
                        buffer_.data() + sampleOffset,
                        sampleCount * sizeof(float));

            bufferReadPos_ += toRead;
            totalRead += toRead;
        } else {
            // バッファを補充
            size_t filled = fillBuffer();
            if (filled == 0) {
                eos_ = true;
            }
        }
    }

    return totalRead;
}

size_t AudioStream::fillBuffer() {
    if (!decoder_ || !decoder_->isOpen()) return 0;

    bufferReadPos_ = 0;
    bufferValidFrames_ = decoder_->decode(buffer_.data(), bufferFrames_);
    return bufferValidFrames_;
}

bool AudioStream::seek(size_t framePosition) {
    if (!decoder_ || !decoder_->isOpen()) return false;

    if (!decoder_->seekTo(framePosition)) return false;

    // バッファをリセット
    bufferReadPos_ = 0;
    bufferValidFrames_ = 0;
    eos_ = false;

    return true;
}

void AudioStream::close() {
    if (decoder_) {
        decoder_->close();
        decoder_.reset();
    }
    buffer_.clear();
    bufferReadPos_ = 0;
    bufferValidFrames_ = 0;
    eos_ = false;
}

bool AudioStream::isEndOfStream() const {
    return eos_;
}

bool AudioStream::isOpen() const {
    return decoder_ && decoder_->isOpen();
}

AudioFormat AudioStream::getFormat() const {
    if (decoder_) return decoder_->getFormat();
    return AudioFormat{};
}

size_t AudioStream::getTotalFrames() const {
    if (decoder_) return decoder_->getTotalFrames();
    return 0;
}

size_t AudioStream::getCurrentFrame() const {
    if (!decoder_) return 0;
    // デコーダの位置からバッファ内の未読分を差し引く
    size_t decoderPos = decoder_->getCurrentFrame();
    size_t buffered = (bufferValidFrames_ > bufferReadPos_)
                      ? bufferValidFrames_ - bufferReadPos_ : 0;
    return (decoderPos > buffered) ? decoderPos - buffered : 0;
}

} // namespace ergo::sound
