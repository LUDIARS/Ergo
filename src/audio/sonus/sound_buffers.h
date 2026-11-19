#pragma once
/// The loaded sounds of ergo_audio's Sonus backend as Sonus static user buffers.
/// One buffer per sound, made once (create) and played any number of times at
/// once, each play with its own volume and pitch (snd_play_user_buffer_ex,
/// Sonus D-104); nothing is copied per play. The sample memory belongs to the
/// game until Sonus releases it:
///   release()  -> snd_destroy_user_buffer (stops the plays still reading it)
///   reclaim()  -> after snd_update(): free the memory of buffers Sonus released
/// Owner thread only (the thread that created the snd_system).
#include "../wav_pcm.h"

#include "snd/sonus.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ergo::audio::sonus_detail {

class SoundBuffers {
public:
    /// `max_ratio`: snd_config.max_pitch_ratio; `internal_rate`: snd_config.sample_rate.
    SoundBuffers(snd_system* sys, uint32_t internal_rate, float max_ratio);
    SoundBuffers(const SoundBuffers&)            = delete;
    SoundBuffers& operator=(const SoundBuffers&) = delete;

    /// The static buffer of sound `id`. false with `error` when Sonus has no free buffer.
    bool create(uint32_t id, std::shared_ptr<const detail::PcmSound> pcm, std::string& error);
    bool has(uint32_t id) const { return live_.count(id) != 0; }
    /// Destroys the buffer (its plays stop); the memory is kept until Sonus releases it.
    void release(uint32_t id);

    /// One play of sound `id`. `volume` in [0, 1], `pitch` > 0 (the Engine contract's
    /// clamping is the caller's). false with `error` when it cannot start.
    bool play(uint32_t id, float volume, float pitch, std::string& error);
    /// on_end of a playback (inside snd_update).
    void ended(snd_playback_handle playback);
    /// After snd_update(): free released buffers, retry destroys the command arena refused.
    void reclaim();

    std::size_t active_plays() const { return plays_.size(); }
    /// Buffers whose memory is held (loaded, or destroyed and not yet released).
    std::size_t held() const { return live_.size() + dying_.size(); }
    /// After snd_destroy(): Sonus references nothing any more.
    void drop_all();

private:
    struct Buffer {
        std::shared_ptr<const detail::PcmSound> pcm;
        std::vector<float*>                     channels;
        snd_user_buffer_handle                  handle{0};
        bool                                    destroyed = false;   ///< snd_destroy_user_buffer recorded
    };
    void destroy(Buffer& b);

    snd_system*                                         sys_;
    uint32_t                                            internal_rate_;
    float                                               max_ratio_;
    std::unordered_map<uint32_t, std::unique_ptr<Buffer>> live_;
    std::vector<std::unique_ptr<Buffer>>                dying_;
    std::unordered_set<uint64_t>                        plays_;   ///< playback handles until on_end
};

} // namespace ergo::audio::sonus_detail
