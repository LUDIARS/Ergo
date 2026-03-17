#pragma once

#include "ergo/input/input_device.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace ergo::input {

class PollingThread {
public:
    PollingThread();
    ~PollingThread();

    void start(IInputDevice* device, uint64_t intervalUs);
    void stop();

    bool isRunning() const;

    void setInterval(uint64_t intervalUs);

private:
    void loop();

    IInputDevice* device_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> intervalUs_{1000};
    std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace ergo::input
