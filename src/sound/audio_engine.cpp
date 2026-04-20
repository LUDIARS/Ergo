// AudioEngine — 高優先度オーディオエンジン実装

#include "ergo/sound/audio_engine.h"
#include <algorithm>
#include <cstring>
#include <cmath>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX  // windows.h's min/max macros clobber std::min/std::max
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#include <sched.h>
#endif

namespace ergo::sound {

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize(const AudioEngineConfig& config) {
    if (running_.load()) return false;

    config_ = config;
    voices_.resize(config.maxVoices);
    for (auto& v : voices_) {
        v.id = InvalidVoiceId;
        v.state = VoiceState::Stopped;
    }

    running_.store(true);
    audioThread_ = std::thread(&AudioEngine::audioThreadFunc, this);

    return true;
}

void AudioEngine::shutdown() {
    if (!running_.load()) return;

    running_.store(false);
    if (audioThread_.joinable()) {
        audioThread_.join();
    }

    std::lock_guard<std::mutex> lock(voicesMutex_);
    voices_.clear();
}

bool AudioEngine::isRunning() const {
    return running_.load();
}

VoiceId AudioEngine::play(std::shared_ptr<AudioStream> stream, float volume, bool loop) {
    if (!running_.load() || !stream) return InvalidVoiceId;

    std::lock_guard<std::mutex> lock(voicesMutex_);
    Voice* voice = findFreeVoice();
    if (!voice) return InvalidVoiceId;

    voice->id = nextVoiceId_++;
    voice->stream = std::move(stream);
    voice->volume = volume;
    voice->loop = loop;
    voice->state = VoiceState::Playing;

    return voice->id;
}

void AudioEngine::stop(VoiceId id) {
    std::lock_guard<std::mutex> lock(voicesMutex_);
    for (auto& v : voices_) {
        if (v.id == id) {
            v.state = VoiceState::Stopped;
            v.stream.reset();
            v.id = InvalidVoiceId;
            return;
        }
    }
}

void AudioEngine::pause(VoiceId id) {
    std::lock_guard<std::mutex> lock(voicesMutex_);
    for (auto& v : voices_) {
        if (v.id == id && v.state == VoiceState::Playing) {
            v.state = VoiceState::Paused;
            return;
        }
    }
}

void AudioEngine::resume(VoiceId id) {
    std::lock_guard<std::mutex> lock(voicesMutex_);
    for (auto& v : voices_) {
        if (v.id == id && v.state == VoiceState::Paused) {
            v.state = VoiceState::Playing;
            return;
        }
    }
}

void AudioEngine::stopAll() {
    std::lock_guard<std::mutex> lock(voicesMutex_);
    for (auto& v : voices_) {
        v.state = VoiceState::Stopped;
        v.stream.reset();
        v.id = InvalidVoiceId;
    }
}

VoiceState AudioEngine::getVoiceState(VoiceId id) const {
    std::lock_guard<std::mutex> lock(voicesMutex_);
    for (const auto& v : voices_) {
        if (v.id == id) return v.state;
    }
    return VoiceState::Stopped;
}

void AudioEngine::setVolume(VoiceId id, float volume) {
    std::lock_guard<std::mutex> lock(voicesMutex_);
    for (auto& v : voices_) {
        if (v.id == id) {
            v.volume = std::max(0.0f, std::min(volume, 2.0f));
            return;
        }
    }
}

void AudioEngine::setMasterVolume(float volume) {
    masterVolume_.store(std::max(0.0f, std::min(volume, 2.0f)));
}

float AudioEngine::getMasterVolume() const {
    return masterVolume_.load();
}

uint32_t AudioEngine::getActiveVoiceCount() const {
    std::lock_guard<std::mutex> lock(voicesMutex_);
    uint32_t count = 0;
    for (const auto& v : voices_) {
        if (v.state == VoiceState::Playing) ++count;
    }
    return count;
}

void AudioEngine::setOutputCallback(AudioCallback callback) {
    outputCallback_ = std::move(callback);
}

const AudioEngineConfig& AudioEngine::getConfig() const {
    return config_;
}

void AudioEngine::audioThreadFunc() {
    setThreadPriority(config_.threadPriority);

    size_t bufferSize = config_.bufferFrames * config_.channels;
    std::vector<float> outputBuffer(bufferSize);

    while (running_.load()) {
        std::memset(outputBuffer.data(), 0, bufferSize * sizeof(float));

        mixVoices(outputBuffer.data(), config_.bufferFrames);

        // マスター音量を適用
        float master = masterVolume_.load();
        if (master != 1.0f) {
            for (size_t i = 0; i < bufferSize; ++i) {
                outputBuffer[i] *= master;
            }
        }

        // 出力コールバックに渡す
        if (outputCallback_) {
            outputCallback_(outputBuffer.data(), config_.bufferFrames, config_.channels);
        }

        // オーディオバッファ周期に合わせたスリープ
        // bufferFrames / sampleRate 秒分待機
        double bufferDuration = static_cast<double>(config_.bufferFrames) /
                                static_cast<double>(config_.sampleRate);
        auto sleepTime = std::chrono::microseconds(
            static_cast<int64_t>(bufferDuration * 1000000.0 * 0.8) // 80%の時間スリープ
        );
        std::this_thread::sleep_for(sleepTime);
    }
}

void AudioEngine::mixVoices(float* outputBuffer, size_t frameCount) {
    std::lock_guard<std::mutex> lock(voicesMutex_);

    std::vector<float> voiceBuffer(frameCount * config_.channels);

    for (auto& voice : voices_) {
        if (voice.state != VoiceState::Playing || !voice.stream) continue;

        size_t framesRead = voice.stream->read(voiceBuffer.data(), frameCount);

        // ボイス音量を適用してミキシング
        for (size_t i = 0; i < framesRead * config_.channels; ++i) {
            outputBuffer[i] += voiceBuffer[i] * voice.volume;
        }

        // ストリーム終端処理
        if (voice.stream->isEndOfStream()) {
            if (voice.loop) {
                voice.stream->seek(0);
            } else {
                voice.state = VoiceState::Stopped;
                voice.stream.reset();
                voice.id = InvalidVoiceId;
            }
        }
    }

    // クリッピング防止
    for (size_t i = 0; i < frameCount * config_.channels; ++i) {
        outputBuffer[i] = std::max(-1.0f, std::min(outputBuffer[i], 1.0f));
    }
}

void AudioEngine::setThreadPriority(int priority) {
#ifdef _WIN32
    // Windows: スレッド優先度をリアルタイムクラスに設定
    int winPriority = THREAD_PRIORITY_NORMAL;
    if (priority >= 80) winPriority = THREAD_PRIORITY_TIME_CRITICAL;
    else if (priority >= 60) winPriority = THREAD_PRIORITY_HIGHEST;
    else if (priority >= 40) winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
    SetThreadPriority(GetCurrentThread(), winPriority);
#elif defined(__linux__)
    // Linux: SCHED_FIFOでリアルタイムスケジューリング
    struct sched_param param{};
    param.sched_priority = std::min(priority, sched_get_priority_max(SCHED_FIFO));
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
#elif defined(__APPLE__)
    // macOS: pthreadの優先度設定
    struct sched_param param{};
    param.sched_priority = std::min(priority, sched_get_priority_max(SCHED_FIFO));
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
#endif
    // 設定失敗は無視（権限不足の場合がある）
}

Voice* AudioEngine::findFreeVoice() {
    for (auto& v : voices_) {
        if (v.state == VoiceState::Stopped && v.id == InvalidVoiceId) {
            return &v;
        }
    }
    return nullptr;
}

} // namespace ergo::sound
