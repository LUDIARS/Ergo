#pragma once

#include "ergo/input/types.h"
#include <vector>
#include <unordered_map>
#include <mutex>

namespace ergo::input {

class InputBuffer {
public:
    explicit InputBuffer(uint32_t capacity = 1024);

    void push(const InputEvent& event);
    void clear();

    uint32_t size() const;
    uint32_t capacity() const;

    const TimedInputEntry& at(uint32_t index) const;

    Duration holdDuration(DeviceType type, uint16_t code) const;

    bool matchSequence(const std::vector<uint16_t>& codes, Duration window) const;

    uint64_t nextSequence();

private:
    std::vector<TimedInputEntry> ring_;
    uint32_t head_ = 0;
    uint32_t count_ = 0;
    uint32_t capacity_;
    uint64_t sequenceCounter_ = 0;

    struct PressRecord {
        TimePoint startTime;
    };
    std::unordered_map<uint64_t, PressRecord> pressMap_;
    mutable std::mutex mutex_;

    uint64_t makeKey(DeviceType type, uint16_t code) const;
};

} // namespace ergo::input
