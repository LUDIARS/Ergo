#pragma once
/// A Sonus output device (snd/ext.h function table) that plays nothing: a
/// timer thread asks Sonus for one period every period length and drops the
/// samples, keeping the largest absolute sample it saw (output_peak). For
/// machines without a sound card (CI, headless servers) and for tests; chosen
/// explicitly with ergo::audio::sonus::Output::Silent, never as a fallback.
#include "snd/ext.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace ergo::audio::sonus_detail {

class SilentOutputDevice {
public:
    SilentOutputDevice();
    ~SilentOutputDevice();   ///< stops the timer thread if Sonus did not
    SilentOutputDevice(const SilentOutputDevice&)            = delete;
    SilentOutputDevice& operator=(const SilentOutputDevice&) = delete;

    /// The table to put in snd_config.output.device. Valid while this object lives.
    const snd_output_device* table() const { return &table_; }

    /// Largest |sample| delivered since the last reset_output_peak().
    float output_peak() const { return peak_.load(std::memory_order_acquire); }
    void  reset_output_peak() { peak_.store(0.0f, std::memory_order_release); }
    /// Periods delivered since open.
    uint64_t periods() const { return periods_.load(std::memory_order_acquire); }

private:
    static int32_t SND_CALL  open_fn(void* u, const snd_device_desc* d, snd_device_callback cb, void* cb_user);
    static int32_t SND_CALL  start_fn(void* u);
    static void SND_CALL     stop_fn(void* u);
    static void SND_CALL     close_fn(void* u);
    static uint32_t SND_CALL period_fn(void* u);
    static uint32_t SND_CALL rate_fn(void* u);
    static uint32_t SND_CALL channels_fn(void* u);
    static double SND_CALL   latency_fn(void* u);
    static void SND_CALL     listener_fn(void* u, snd_device_event_fn fn, void* fn_user);

    bool start();
    void stop();
    void run();

    snd_output_device   table_{};
    snd_device_callback cb_      = nullptr;
    void*               cb_user_ = nullptr;
    uint32_t            rate_    = 48000;
    uint32_t            channels_ = 2;
    uint32_t            period_  = 256;
    std::vector<float>  buffer_;

    std::mutex              mtx_;
    std::condition_variable cv_;
    bool                    running_ = false;
    std::thread             thread_;
    std::atomic<float>      peak_{0.0f};
    std::atomic<uint64_t>   periods_{0};
};

} // namespace ergo::audio::sonus_detail
