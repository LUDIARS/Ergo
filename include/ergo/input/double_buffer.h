#pragma once

#include <atomic>
#include <array>

namespace ergo::input {

template <typename T>
class DoubleBuffer {
public:
    void write(const T& data) {
        buffers_[writeIndex_.load(std::memory_order_relaxed)] = data;
    }

    T& writableRef() {
        return buffers_[writeIndex_.load(std::memory_order_relaxed)];
    }

    const T& read() const {
        return buffers_[1 - writeIndex_.load(std::memory_order_acquire)];
    }

    void swap() {
        int oldWrite = writeIndex_.load(std::memory_order_relaxed);
        int newWrite = 1 - oldWrite;
        writeIndex_.store(newWrite, std::memory_order_release);
        // Copy latest state to new write buffer so state persists across frames
        buffers_[newWrite] = buffers_[oldWrite];
    }

private:
    std::array<T, 2> buffers_{};
    std::atomic<int> writeIndex_{0};
};

} // namespace ergo::input
