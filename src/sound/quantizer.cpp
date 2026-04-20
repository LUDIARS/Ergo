// Quantizer — クォンタイズシステム実装

#include "ergo/sound/quantizer.h"
#include <cmath>
#include <algorithm>

namespace ergo::sound {

Quantizer::Quantizer() {
    recalculate();
}

Quantizer::Quantizer(const QuantizeConfig& config)
    : config_(config) {
    recalculate();
}

void Quantizer::setBPM(double bpm) {
    config_.bpm = bpm;
    recalculate();
}

double Quantizer::getBPM() const {
    return config_.bpm;
}

void Quantizer::setSampleRate(uint32_t sampleRate) {
    config_.sampleRate = sampleRate;
    recalculate();
}

void Quantizer::setGridSize(GridSize gridSize) {
    config_.gridSize = gridSize;
    recalculate();
}

GridSize Quantizer::getGridSize() const {
    return config_.gridSize;
}

void Quantizer::recalculate() {
    // 1拍（4分音符）あたりのサンプル数
    // sampleRate / (bpm / 60) = sampleRate * 60 / bpm
    samplesPerBeat_ = static_cast<double>(config_.sampleRate) * 60.0 / config_.bpm;

    // 1グリッドあたりのサンプル数
    // gridSize は4分音符を基準とした分割数
    // Quarter(4) = 1拍, Eighth(8) = 0.5拍, Sixteenth(16) = 0.25拍
    double gridDivisor = static_cast<double>(static_cast<uint8_t>(config_.gridSize)) / 4.0;
    samplesPerGrid_ = samplesPerBeat_ / gridDivisor;
}

double Quantizer::getSamplesPerGrid() const {
    return samplesPerGrid_;
}

double Quantizer::getSamplesPerBeat() const {
    return samplesPerBeat_;
}

size_t Quantizer::quantize(size_t samplePosition) const {
    if (samplesPerGrid_ <= 0.0) return samplePosition;

    double gridIndex = static_cast<double>(samplePosition) / samplesPerGrid_;
    double nearest = std::round(gridIndex);
    return static_cast<size_t>(nearest * samplesPerGrid_);
}

size_t Quantizer::getNextGridPoint(size_t samplePosition) const {
    if (samplesPerGrid_ <= 0.0) return samplePosition;

    double gridIndex = static_cast<double>(samplePosition) / samplesPerGrid_;
    double next = std::floor(gridIndex) + 1.0;
    return static_cast<size_t>(next * samplesPerGrid_);
}

size_t Quantizer::getPreviousGridPoint(size_t samplePosition) const {
    if (samplesPerGrid_ <= 0.0) return samplePosition;

    double gridIndex = static_cast<double>(samplePosition) / samplesPerGrid_;
    double prev = std::ceil(gridIndex) - 1.0;
    if (prev < 0.0) prev = 0.0;
    return static_cast<size_t>(prev * samplesPerGrid_);
}

double Quantizer::samplesToBeats(size_t samples) const {
    if (samplesPerBeat_ <= 0.0) return 0.0;
    return static_cast<double>(samples) / samplesPerBeat_;
}

size_t Quantizer::beatsToSamples(double beats) const {
    return static_cast<size_t>(beats * samplesPerBeat_);
}

double Quantizer::samplesToBars(size_t samples) const {
    // 4/4拍子: 1小節 = 4拍
    return samplesToBeats(samples) / 4.0;
}

size_t Quantizer::barsToSamples(double bars) const {
    return beatsToSamples(bars * 4.0);
}

} // namespace ergo::sound
