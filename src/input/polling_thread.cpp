#include "ergo/input/polling_thread.h"

namespace ergo::input {

PollingThread::PollingThread() = default;

PollingThread::~PollingThread() {
    stop();
}

void PollingThread::start(IInputDevice* device, uint64_t intervalUs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_.load(std::memory_order_relaxed)) return;

    device_ = device;
    intervalUs_.store(intervalUs, std::memory_order_relaxed);
    running_.store(true, std::memory_order_release);
    thread_ = std::thread(&PollingThread::loop, this);
}

void PollingThread::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_.load(std::memory_order_relaxed)) return;
        running_.store(false, std::memory_order_release);
        cv_.notify_all();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool PollingThread::isRunning() const {
    return running_.load(std::memory_order_acquire);
}

void PollingThread::setInterval(uint64_t intervalUs) {
    intervalUs_.store(intervalUs, std::memory_order_relaxed);
    intervalChanged_.store(true, std::memory_order_relaxed);
    cv_.notify_all();
}

void PollingThread::loop() {
    while (running_.load(std::memory_order_acquire)) {
        if (device_) {
            device_->poll();
        }

        std::unique_lock<std::mutex> lock(mutex_);
        // wait_for(pred) computes its wake deadline once from the interval at
        // call time. A plain notify_all() from setInterval() only causes an
        // early recheck of the predicate; if the predicate is still false it
        // keeps sleeping until that original deadline, so a shorter interval
        // set mid-sleep would not take effect until the *next* cycle. Fold
        // the "interval changed" flag into the predicate so setInterval()
        // reliably wakes this loop immediately and the new interval applies
        // to the very next wait.
        cv_.wait_for(lock,
                     std::chrono::microseconds(intervalUs_.load(std::memory_order_relaxed)),
                     [this] {
                         return !running_.load(std::memory_order_relaxed) ||
                                intervalChanged_.exchange(false, std::memory_order_relaxed);
                     });
    }
}

} // namespace ergo::input
