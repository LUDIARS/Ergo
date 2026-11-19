#include "silent_output_device.h"

#include <chrono>
#include <cmath>

namespace ergo::audio::sonus_detail {
namespace {
SilentOutputDevice& self(void* u) { return *static_cast<SilentOutputDevice*>(u); }
} // namespace

SilentOutputDevice::SilentOutputDevice() {
    table_.user                   = this;
    table_.open                   = &open_fn;
    table_.start                  = &start_fn;
    table_.stop                   = &stop_fn;
    table_.close                  = &close_fn;
    table_.period_frames          = &period_fn;
    table_.sample_rate            = &rate_fn;
    table_.channels               = &channels_fn;
    table_.output_latency_seconds = &latency_fn;
    table_.set_event_listener     = &listener_fn;
}

SilentOutputDevice::~SilentOutputDevice() { stop(); }

int32_t SND_CALL SilentOutputDevice::open_fn(void* u, const snd_device_desc* d, snd_device_callback cb, void* cb_user) {
    SilentOutputDevice& s = self(u);
    if (cb == nullptr || d == nullptr) return 0;
    if (d->sample_rate != 0) s.rate_ = d->sample_rate;
    if (d->channels != 0) s.channels_ = d->channels;
    if (d->period_frames != 0) s.period_ = d->period_frames;
    s.cb_      = cb;
    s.cb_user_ = cb_user;
    s.buffer_.assign(static_cast<std::size_t>(s.period_) * s.channels_, 0.0f);
    s.periods_.store(0, std::memory_order_release);
    return 1;
}

int32_t SND_CALL SilentOutputDevice::start_fn(void* u) { return self(u).start() ? 1 : 0; }
void SND_CALL    SilentOutputDevice::stop_fn(void* u) { self(u).stop(); }

void SND_CALL SilentOutputDevice::close_fn(void* u) {
    SilentOutputDevice& s = self(u);
    s.stop();
    s.cb_      = nullptr;
    s.cb_user_ = nullptr;
}

uint32_t SND_CALL SilentOutputDevice::period_fn(void* u) { return self(u).period_; }
uint32_t SND_CALL SilentOutputDevice::rate_fn(void* u) { return self(u).rate_; }
uint32_t SND_CALL SilentOutputDevice::channels_fn(void* u) { return self(u).channels_; }
double SND_CALL   SilentOutputDevice::latency_fn(void*) { return 0.0; }
void SND_CALL     SilentOutputDevice::listener_fn(void*, snd_device_event_fn, void*) {}   // no route changes

bool SilentOutputDevice::start() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (running_) return true;
    if (cb_ == nullptr) return false;
    running_ = true;
    thread_  = std::thread([this] { run(); });
    return true;
}

void SilentOutputDevice::stop() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!running_) return;
        running_ = false;
    }
    cv_.notify_all();
    thread_.join();   // "returns after the last callback finished" (snd/ext.h)
}

void SilentOutputDevice::run() {
    using Clock = std::chrono::steady_clock;
    const auto period = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(static_cast<double>(period_) / static_cast<double>(rate_)));
    auto next = Clock::now();
    std::unique_lock<std::mutex> lk(mtx_);
    while (running_) {
        lk.unlock();
        cb_(cb_user_, buffer_.data(), period_, channels_);
        float m = 0.0f;
        for (float v : buffer_) m = std::fmax(m, std::fabs(v));
        float cur = peak_.load(std::memory_order_acquire);
        while (m > cur && !peak_.compare_exchange_weak(cur, m, std::memory_order_acq_rel)) {
        }
        periods_.fetch_add(1, std::memory_order_acq_rel);
        next += period;
        lk.lock();
        cv_.wait_until(lk, next, [this] { return !running_; });
    }
}

} // namespace ergo::audio::sonus_detail
