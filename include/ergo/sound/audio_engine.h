#pragma once

#include "ergo/sound/types.h"
#include "ergo/sound/audio_stream.h"
#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace ergo::sound {

// =====================================================================
// Voice — 再生ボイス（内部管理用）
// =====================================================================
struct Voice {
    VoiceId                         id      = InvalidVoiceId;
    std::shared_ptr<AudioStream>    stream;
    VoiceState                      state   = VoiceState::Stopped;
    float                           volume  = 1.0f;
    float                           pan     = 0.0f;  // -1.0(左) 〜 1.0(右)
    bool                            loop    = false;
};

// =====================================================================
// AudioEngine — 高優先度オーディオエンジン
// =====================================================================
/// 高優先度スレッドでオーディオミキシングを行い、リアルタイム再生する。
/// ボイス管理、ミキシング、出力を担当する。
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    /// エンジンを初期化する
    bool initialize(const AudioEngineConfig& config);

    /// エンジンを終了する
    void shutdown();

    /// エンジンが稼働中か
    bool isRunning() const;

    /// ストリームを再生する
    /// @return ボイスID（失敗時はInvalidVoiceId）
    VoiceId play(std::shared_ptr<AudioStream> stream, float volume = 1.0f, bool loop = false);

    /// ボイスを停止する
    void stop(VoiceId id);

    /// ボイスを一時停止する
    void pause(VoiceId id);

    /// ボイスを再開する
    void resume(VoiceId id);

    /// 全ボイスを停止する
    void stopAll();

    /// ボイスの状態を取得する
    VoiceState getVoiceState(VoiceId id) const;

    /// ボイスの音量を設定する
    void setVolume(VoiceId id, float volume);

    /// マスター音量を設定する
    void setMasterVolume(float volume);

    /// マスター音量を取得する
    float getMasterVolume() const;

    /// アクティブなボイス数を取得する
    uint32_t getActiveVoiceCount() const;

    /// オーディオコールバックを設定する（外部出力用）
    void setOutputCallback(AudioCallback callback);

    /// 設定を取得する
    const AudioEngineConfig& getConfig() const;

private:
    AudioEngineConfig               config_;
    std::vector<Voice>              voices_;
    mutable std::mutex              voicesMutex_;
    std::atomic<bool>               running_{false};
    std::thread                     audioThread_;
    std::atomic<float>              masterVolume_{1.0f};
    AudioCallback                   outputCallback_;
    VoiceId                         nextVoiceId_ = 1;

    /// オーディオスレッドのメインループ
    void audioThreadFunc();

    /// ミキシング処理
    void mixVoices(float* outputBuffer, size_t frameCount);

    /// スレッド優先度を設定する
    void setThreadPriority(int priority);

    /// 空きボイススロットを取得する
    Voice* findFreeVoice();
};

} // namespace ergo::sound
