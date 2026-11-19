#pragma once
/// One-shot plays of ergo_audio's Sonus backend. Sonus has no per-playback
/// volume or pitch for game-owned samples, so every play gets its own static
/// user buffer: the sound's samples scaled by the play's volume, played at
/// source rate * pitch (Sonus resamples). The buffer memory belongs to the
/// game until Sonus releases it:
///   start()          -> snd_create_user_buffer + snd_play_user_buffer (recorded)
///   ended()          -> on_end of the playback: snd_destroy_user_buffer
///   reclaim()        -> after snd_update(): free buffers Sonus released
/// Owner thread only (the thread that created the snd_system).
#include "../wav_pcm.h"

#include "snd/sonus.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ergo::audio::sonus_detail {

class PlayBuffers {
public:
    /// `internal_rate`: snd_config.sample_rate; `max_ratio`: snd_config.max_pitch_ratio.
    PlayBuffers(snd_system* sys, uint32_t internal_rate, float max_ratio);
    PlayBuffers(const PlayBuffers&)            = delete;
    PlayBuffers& operator=(const PlayBuffers&) = delete;

    /// `volume` in [0, 1], `pitch` > 0 (the caller clamps as the Engine contract says).
    /// false with `error` when Sonus has no free buffer / playback slot.
    bool start(const std::shared_ptr<const detail::PcmSound>& pcm, float volume, float pitch, std::string& error);
    /// on_end of a playback (inside snd_update). false if it is not one of ours.
    bool ended(snd_playback_handle playback);
    /// After snd_update(): destroy ended buffers still to destroy, free released ones.
    void reclaim();
    /// Plays whose memory is still held (playing, or waiting for Sonus to release it).
    std::size_t held() const { return plays_.size(); }
    /// After snd_destroy(): Sonus references nothing any more.
    void drop_all() { plays_.clear(); }

private:
    struct Play {
        std::vector<float>     samples;
        std::vector<float*>    channels;
        snd_user_buffer_handle buffer{0};
        snd_playback_handle    playback{0};
        bool                   ended     = false;   ///< on_end seen (or never started)
        bool                   destroyed = false;   ///< snd_destroy_user_buffer recorded
    };
    void destroy(Play& p);

    snd_system*                        sys_;
    uint32_t                           internal_rate_;
    float                              max_ratio_;
    std::vector<std::unique_ptr<Play>> plays_;
};

} // namespace ergo::audio::sonus_detail
