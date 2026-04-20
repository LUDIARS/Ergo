#include "ergo/frame/counter.h"
#include "ergo/frame/frame.h"

#include <cstdio>

namespace ergo::frame {

Counter& Counter::instance() {
    static Counter c;
    return c;
}

void Counter::tick() {
    const TimePoint now = Clock::now();
    if (has_prev_) {
        const auto d = std::chrono::duration<float>(now - last_tick_).count();
        last_dt_ = d;
        dt_window_.push_back(d);
        while (dt_window_.size() > window_size_) dt_window_.pop_front();
    } else {
        has_prev_ = true;
        last_dt_  = 0.0f;
    }
    last_tick_ = now;
    ++count_;
}

void Counter::reset() {
    count_       = 0;
    has_prev_    = false;
    last_dt_     = 0.0f;
    dt_window_.clear();
}

float Counter::fps() const {
    if (dt_window_.empty()) return 0.0f;
    float sum = 0.0f;
    for (float d : dt_window_) sum += d;
    if (sum <= 0.0f) return 0.0f;
    return static_cast<float>(dt_window_.size()) / sum;
}

void Counter::set_window_size(std::size_t n) {
    if (n < 1) n = 1;
    window_size_ = n;
    while (dt_window_.size() > window_size_) dt_window_.pop_front();
}

// ---- free-function HUD formatter ----------------------------------------

std::string format_hud() {
    const Counter& c   = Counter::instance();
    const float    f   = c.fps();
    const float    dms = c.dt_seconds() * 1000.0f;
    char buf[96];
    std::snprintf(buf, sizeof(buf), "F%llu FPS=%.1f dt=%.2fms",
                  static_cast<unsigned long long>(c.count()), f, dms);
    return std::string(buf);
}

} // namespace ergo::frame
