#pragma once
/// lesson_action_feel — keeps a loop at a fixed rate of real time (60 Hz by
/// default). After a stall it does not burst to catch up: the next frame is
/// simply scheduled from now. Each frame advances the game by the fixed step,
/// so the simulation stays deterministic whatever the machine's speed.
#include <chrono>

namespace lesson_action_feel {

class FramePacer {
public:
    using Clock = std::chrono::steady_clock;

    explicit FramePacer(double hz = 60.0);

    float  step_seconds() const { return step_; }
    /// Real seconds since construction.
    double elapsed() const;
    /// Sleep until the next frame is due.
    void wait_next();

private:
    float             step_;
    Clock::duration   period_;
    Clock::time_point start_;
    Clock::time_point next_;
};

} // namespace lesson_action_feel
