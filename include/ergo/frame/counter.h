#pragma once

/// Counter — per-frame tick / FPS tracker.
///
/// Usage:
///   // Once per frame in the host's main loop:
///   ergo::frame::Counter::instance().tick();
///
///   // Anywhere:
///   auto n   = ergo::frame::Counter::instance().count();
///   auto fps = ergo::frame::Counter::instance().fps();
///
/// FPS is a rolling arithmetic mean over the last `window_size()` frames
/// of `dt`. Zero is returned until at least one frame has elapsed.
///
/// Expected to be driven from the host's main thread — Counter itself
/// holds no synchronisation.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>

namespace ergo::frame {

class Counter {
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    static Counter& instance();

    Counter(const Counter&)            = delete;
    Counter& operator=(const Counter&) = delete;

    /// Call once per frame. Increments the counter, samples the clock,
    /// maintains the rolling dt window, updates FPS.
    void tick();

    /// Back to zero. Clears the dt window and the FPS baseline.
    void reset();

    uint64_t count()      const { return count_; }
    float    fps()        const;
    float    dt_seconds() const { return last_dt_; }

    /// Rolling window size for FPS (default 60 frames, minimum 1).
    void   set_window_size(std::size_t n);
    std::size_t window_size() const { return window_size_; }

private:
    Counter() = default;

    uint64_t        count_       = 0;
    bool            has_prev_    = false;
    TimePoint       last_tick_{};
    float           last_dt_     = 0.0f;
    std::deque<float> dt_window_;
    std::size_t     window_size_ = 60;
};

} // namespace ergo::frame
