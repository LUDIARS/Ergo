#include "sound_buffers.h"

#include "result_text.h"

#include <cstdio>

namespace ergo::audio::sonus_detail {
namespace {
// Sonus rounds a playback's pitch into [1/16, 16] (D-104).
constexpr float kMinPitch = 1.0f / 16.0f;
constexpr float kMaxPitch = 16.0f;
} // namespace

SoundBuffers::SoundBuffers(snd_system* sys, uint32_t internal_rate, float max_ratio)
    : sys_(sys), internal_rate_(internal_rate), max_ratio_(max_ratio) {}

bool SoundBuffers::create(uint32_t id, std::shared_ptr<const detail::PcmSound> pcm, std::string& error) {
    auto b = std::make_unique<Buffer>();
    b->pcm = std::move(pcm);
    // Sonus only reads static memory (it never writes it), so the const samples are handed over as is.
    float* base = const_cast<float*>(b->pcm->samples.data());
    for (uint32_t c = 0; c < b->pcm->channels; ++c) b->channels.push_back(base + std::size_t{c} * b->pcm->frames);
    snd_user_buffer_desc d{};
    d.channels      = b->channels.data();
    d.channel_count = b->pcm->channels;
    d.frames        = b->pcm->frames;
    d.sample_rate   = b->pcm->sample_rate;   // Sonus resamples to its rate, times each play's pitch
    d.mode          = SND_USER_BUFFER_STATIC;
    d.loop          = 0;
    b->handle       = snd_create_user_buffer(sys_, &d);
    if (b->handle.value == SND_HANDLE_INVALID_VALUE) {
        error = "no free Sonus user buffer (too many sounds loaded; raise sonus::Options::max_loaded_sounds)";
        return false;   // nothing was created: the memory can go now
    }
    live_[id] = std::move(b);
    return true;
}

void SoundBuffers::release(uint32_t id) {
    auto it = live_.find(id);
    if (it == live_.end()) return;
    destroy(*it->second);
    dying_.push_back(std::move(it->second));
    live_.erase(it);
}

bool SoundBuffers::play(uint32_t id, float volume, float pitch, std::string& error) {
    auto it = live_.find(id);
    if (it == live_.end()) {
        error = "the sound has no Sonus buffer";
        return false;
    }
    const detail::PcmSound& pcm = *it->second->pcm;
    if (pitch < kMinPitch || pitch > kMaxPitch) {
        std::fprintf(stderr, "[audio:sonus] pitch %.3f is outside Sonus' playback pitch range [1/16, 16]; clamped\n",
                     static_cast<double>(pitch));
    }
    const float p     = pitch < kMinPitch ? kMinPitch : pitch > kMaxPitch ? kMaxPitch : pitch;
    const float ratio = p * static_cast<float>(pcm.sample_rate) / static_cast<float>(internal_rate_);
    if (ratio > max_ratio_) {
        std::fprintf(stderr,
                     "[audio:sonus] pitch %.2f of a %u Hz sound exceeds Sonus' resampling limit (ratio %.0f); clamped\n",
                     static_cast<double>(pitch), static_cast<unsigned>(pcm.sample_rate), static_cast<double>(max_ratio_));
    }
    const snd_playback_handle h =
        snd_play_user_buffer_ex(sys_, it->second->handle, SND_ID_NONE, volume, p, snd_emitter_handle{0}, nullptr);
    if (h.value == SND_HANDLE_INVALID_VALUE) {
        error = "no free Sonus playback slot";
        return false;
    }
    plays_.insert(h.value);
    return true;
}

void SoundBuffers::ended(snd_playback_handle playback) { plays_.erase(playback.value); }

void SoundBuffers::destroy(Buffer& b) {
    if (b.destroyed) return;
    const snd_result r = snd_destroy_user_buffer(sys_, b.handle);
    if (r == SND_OK) {
        b.destroyed = true;
    } else if (r != SND_ERR_COMMAND_BUFFER_FULL) {   // full: retried by reclaim()
        std::fprintf(stderr, "[audio:sonus] snd_destroy_user_buffer failed: %s\n", result_name(r));
        b.destroyed = true;
    }
}

void SoundBuffers::reclaim() {
    for (std::size_t i = 0; i < dying_.size();) {
        Buffer& b = *dying_[i];
        if (!b.destroyed) destroy(b);
        if (b.destroyed && snd_get_user_buffer_released(sys_, b.handle) == 1) {
            dying_.erase(dying_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        ++i;
    }
}

void SoundBuffers::drop_all() {
    live_.clear();
    dying_.clear();
    plays_.clear();
}

} // namespace ergo::audio::sonus_detail
