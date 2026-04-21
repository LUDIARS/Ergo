// WaveformProcessor — 波形操作プロセッサ実装

#include "ergo/sound/waveform_processor.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace ergo::sound {

void WaveformProcessor::amplify(AudioBuffer& buffer, float gain) {
    for (auto& sample : buffer.data) {
        sample *= gain;
    }
}

void WaveformProcessor::normalize(AudioBuffer& buffer) {
    if (buffer.data.empty()) return;

    float maxAbs = 0.0f;
    for (const auto& sample : buffer.data) {
        float abs = std::fabs(sample);
        if (abs > maxAbs) maxAbs = abs;
    }

    if (maxAbs > 0.0f && maxAbs != 1.0f) {
        float gain = 1.0f / maxAbs;
        amplify(buffer, gain);
    }
}

float WaveformProcessor::calculateFadeCurve(float t, FadeType type) {
    switch (type) {
        case FadeType::Linear:
            return t;
        case FadeType::EaseIn:
            return t * t;
        case FadeType::EaseOut:
            return 1.0f - (1.0f - t) * (1.0f - t);
    }
    return t;
}

void WaveformProcessor::fadeIn(AudioBuffer& buffer, size_t durationFrames, FadeType type) {
    size_t frames = std::min(durationFrames, buffer.frameCount);
    uint16_t channels = buffer.format.channels;

    for (size_t f = 0; f < frames; ++f) {
        float t = static_cast<float>(f) / static_cast<float>(frames);
        float gain = calculateFadeCurve(t, type);
        for (uint16_t ch = 0; ch < channels; ++ch) {
            buffer.data[f * channels + ch] *= gain;
        }
    }
}

void WaveformProcessor::fadeOut(AudioBuffer& buffer, size_t durationFrames, FadeType type) {
    size_t frames = std::min(durationFrames, buffer.frameCount);
    uint16_t channels = buffer.format.channels;
    size_t startFrame = buffer.frameCount - frames;

    for (size_t f = 0; f < frames; ++f) {
        float t = static_cast<float>(f) / static_cast<float>(frames);
        float gain = 1.0f - calculateFadeCurve(t, type);
        size_t idx = (startFrame + f) * channels;
        for (uint16_t ch = 0; ch < channels; ++ch) {
            buffer.data[idx + ch] *= gain;
        }
    }
}

AudioBuffer WaveformProcessor::pitchShift(const AudioBuffer& buffer, float semitones) {
    if (buffer.data.empty()) return buffer;

    // リサンプリングベースのピッチシフト
    // ピッチ比率: 2^(semitones/12)
    double ratio = std::pow(2.0, static_cast<double>(semitones) / 12.0);
    uint16_t channels = buffer.format.channels;
    size_t newFrameCount = static_cast<size_t>(
        static_cast<double>(buffer.frameCount) / ratio
    );

    AudioBuffer result;
    result.format = buffer.format;
    result.resize(newFrameCount);

    for (size_t f = 0; f < newFrameCount; ++f) {
        double srcPos = static_cast<double>(f) * ratio;
        size_t srcIdx = static_cast<size_t>(srcPos);
        double frac = srcPos - static_cast<double>(srcIdx);

        for (uint16_t ch = 0; ch < channels; ++ch) {
            size_t idx0 = srcIdx * channels + ch;
            size_t idx1 = (srcIdx + 1) * channels + ch;

            float s0 = (idx0 < buffer.data.size()) ? buffer.data[idx0] : 0.0f;
            float s1 = (idx1 < buffer.data.size()) ? buffer.data[idx1] : 0.0f;

            // 線形補間
            result.data[f * channels + ch] = s0 + static_cast<float>(frac) * (s1 - s0);
        }
    }

    return result;
}

AudioBuffer WaveformProcessor::mix(const AudioBuffer& a, const AudioBuffer& b, float ratioA) {
    float ratioB = 1.0f - ratioA;
    uint16_t channels = a.format.channels;
    size_t maxFrames = std::max(a.frameCount, b.frameCount);

    AudioBuffer result;
    result.format = a.format;
    result.resize(maxFrames);

    for (size_t i = 0; i < maxFrames * channels; ++i) {
        float sa = (i < a.data.size()) ? a.data[i] : 0.0f;
        float sb = (i < b.data.size()) ? b.data[i] : 0.0f;
        result.data[i] = sa * ratioA + sb * ratioB;
    }

    return result;
}

void WaveformProcessor::applyEnvelope(AudioBuffer& buffer, const std::vector<EnvelopePoint>& points) {
    if (points.empty() || buffer.data.empty()) return;

    uint16_t channels = buffer.format.channels;

    for (size_t f = 0; f < buffer.frameCount; ++f) {
        float amplitude = 1.0f;

        // 現在のフレーム位置に対応するエンベロープ値を線形補間で計算
        if (f <= points.front().framePosition) {
            amplitude = points.front().amplitude;
        } else if (f >= points.back().framePosition) {
            amplitude = points.back().amplitude;
        } else {
            for (size_t p = 0; p + 1 < points.size(); ++p) {
                if (f >= points[p].framePosition && f < points[p + 1].framePosition) {
                    size_t range = points[p + 1].framePosition - points[p].framePosition;
                    if (range > 0) {
                        float t = static_cast<float>(f - points[p].framePosition) /
                                  static_cast<float>(range);
                        amplitude = points[p].amplitude + t * (points[p + 1].amplitude - points[p].amplitude);
                    }
                    break;
                }
            }
        }

        for (uint16_t ch = 0; ch < channels; ++ch) {
            buffer.data[f * channels + ch] *= amplitude;
        }
    }
}

void WaveformProcessor::removeDCOffset(AudioBuffer& buffer) {
    if (buffer.data.empty()) return;

    uint16_t channels = buffer.format.channels;

    // チャンネルごとにDCオフセットを計算して除去
    for (uint16_t ch = 0; ch < channels; ++ch) {
        double sum = 0.0;
        for (size_t f = 0; f < buffer.frameCount; ++f) {
            sum += static_cast<double>(buffer.data[f * channels + ch]);
        }
        float dcOffset = static_cast<float>(sum / static_cast<double>(buffer.frameCount));

        for (size_t f = 0; f < buffer.frameCount; ++f) {
            buffer.data[f * channels + ch] -= dcOffset;
        }
    }
}

void WaveformProcessor::reverse(AudioBuffer& buffer) {
    if (buffer.frameCount < 2) return;

    uint16_t channels = buffer.format.channels;

    for (size_t i = 0; i < buffer.frameCount / 2; ++i) {
        size_t j = buffer.frameCount - 1 - i;
        for (uint16_t ch = 0; ch < channels; ++ch) {
            std::swap(buffer.data[i * channels + ch],
                      buffer.data[j * channels + ch]);
        }
    }
}

} // namespace ergo::sound
