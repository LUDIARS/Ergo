#include "frame_pacer.h"

#include <thread>

namespace lesson_action_feel {

FramePacer::FramePacer(double hz)
    : step_(static_cast<float>(1.0 / hz)),
      period_(std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / hz))),
      start_(Clock::now()),
      next_(start_) {}

double FramePacer::elapsed() const { return std::chrono::duration<double>(Clock::now() - start_).count(); }

void FramePacer::wait_next() {
    next_ += period_;
    const auto now = Clock::now();
    if (next_ < now) next_ = now;   // after a stall: do not burst
    else std::this_thread::sleep_until(next_);
}

} // namespace lesson_action_feel
