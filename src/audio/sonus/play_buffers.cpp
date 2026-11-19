#include "play_buffers.h"

#include "result_text.h"

#include <cmath>
#include <cstdio>

namespace ergo::audio::sonus_detail {

PlayBuffers::PlayBuffers(snd_system* sys, uint32_t internal_rate, float max_ratio)
    : sys_(sys), internal_rate_(internal_rate), max_ratio_(max_ratio) {}

bool PlayBuffers::start(const std::shared_ptr<const detail::PcmSound>& pcm, float volume, float pitch,
                        std::string& error) {
    auto p = std::make_unique<Play>();
    p->samples.resize(pcm->samples.size());
    for (std::size_t i = 0; i < pcm->samples.size(); ++i) p->samples[i] = pcm->samples[i] * volume;
    for (uint32_t c = 0; c < pcm->channels; ++c) p->channels.push_back(p->samples.data() + std::size_t{c} * pcm->frames);

    // Pitch = playing the samples at a different source rate. Sonus resamples a voice at most
    // max_ratio source frames per output frame; above that it would clamp, so clamp here and say so.
    double       rate     = static_cast<double>(pcm->sample_rate) * static_cast<double>(pitch);
    const double max_rate = static_cast<double>(internal_rate_) * static_cast<double>(max_ratio_);
    if (rate > max_rate) {
        std::fprintf(stderr,
                     "[audio:sonus] pitch %.2f exceeds Sonus' resampling limit (%.0f Hz source rate at most); clamped\n",
                     static_cast<double>(pitch), max_rate);
        rate = max_rate;
    }
    if (rate < 1.0) rate = 1.0;

    snd_user_buffer_desc d{};
    d.channels      = p->channels.data();
    d.channel_count = pcm->channels;
    d.frames        = pcm->frames;
    d.sample_rate   = static_cast<uint32_t>(std::lround(rate));
    d.mode          = SND_USER_BUFFER_STATIC;
    d.loop          = 0;
    p->buffer       = snd_create_user_buffer(sys_, &d);
    if (p->buffer.value == SND_HANDLE_INVALID_VALUE) {
        error = "no free Sonus user buffer (too many plays at once; raise sonus::Options::max_concurrent_plays)";
        return false;   // nothing was created: the memory can go now
    }
    p->playback = snd_play_user_buffer(sys_, p->buffer, SND_ID_NONE, snd_emitter_handle{0}, nullptr);
    if (p->playback.value == SND_HANDLE_INVALID_VALUE) {
        error = "no free Sonus playback slot";
        p->ended = true;
        destroy(*p);
        plays_.push_back(std::move(p));   // kept until Sonus releases the buffer
        return false;
    }
    plays_.push_back(std::move(p));
    return true;
}

bool PlayBuffers::ended(snd_playback_handle playback) {
    for (auto& p : plays_) {
        if (p->playback.value == playback.value && !p->ended) {
            p->ended = true;
            destroy(*p);
            return true;
        }
    }
    return false;
}

void PlayBuffers::destroy(Play& p) {
    if (p.destroyed) return;
    const snd_result r = snd_destroy_user_buffer(sys_, p.buffer);
    if (r == SND_OK) {
        p.destroyed = true;
    } else if (r != SND_ERR_COMMAND_BUFFER_FULL) {   // full: retried by reclaim()
        std::fprintf(stderr, "[audio:sonus] snd_destroy_user_buffer failed: %s\n", result_name(r));
        p.destroyed = true;   // a stale handle: Sonus no longer references the memory
    }
}

void PlayBuffers::reclaim() {
    for (std::size_t i = 0; i < plays_.size();) {
        Play& p = *plays_[i];
        if (p.ended && !p.destroyed) destroy(p);
        if (p.destroyed && snd_get_user_buffer_released(sys_, p.buffer) == 1) {
            plays_.erase(plays_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        ++i;
    }
}

} // namespace ergo::audio::sonus_detail
