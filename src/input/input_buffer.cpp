#include "ergo/input/input_buffer.h"
#include <algorithm>

namespace ergo::input {

InputBuffer::InputBuffer(uint32_t capacity)
    : capacity_(capacity) {
    ring_.resize(capacity);
}

void InputBuffer::push(const InputEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    TimedInputEntry entry;
    entry.event = event;
    entry.event.sequence = sequenceCounter_++;

    auto key = makeKey(event.deviceType, event.code);

    if (event.eventType == EventType::Press) {
        pressMap_[key] = PressRecord{event.timestamp};
    } else if (event.eventType == EventType::Release) {
        auto it = pressMap_.find(key);
        if (it != pressMap_.end()) {
            entry.holdDuration = event.timestamp - it->second.startTime;
            pressMap_.erase(it);
        }
    }

    ring_[head_] = entry;
    head_ = (head_ + 1) % capacity_;
    if (count_ < capacity_) {
        ++count_;
    }
}

void InputBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    head_ = 0;
    count_ = 0;
    pressMap_.clear();
}

uint32_t InputBuffer::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

uint32_t InputBuffer::capacity() const {
    return capacity_;
}

const TimedInputEntry& InputBuffer::at(uint32_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t start = (count_ < capacity_) ? 0 : head_;
    uint32_t actualIndex = (start + index) % capacity_;
    return ring_[actualIndex];
}

Duration InputBuffer::holdDuration(DeviceType type, uint16_t code) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = makeKey(type, code);
    auto it = pressMap_.find(key);
    if (it != pressMap_.end()) {
        return Clock::now() - it->second.startTime;
    }
    return Duration{};
}

bool InputBuffer::matchSequence(const std::vector<uint16_t>& codes, Duration window) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (codes.empty() || count_ == 0) return false;

    uint32_t start = (count_ < capacity_) ? 0 : head_;
    size_t matchIdx = 0;
    TimePoint firstMatchTime{};

    for (uint32_t i = 0; i < count_ && matchIdx < codes.size(); ++i) {
        uint32_t idx = (start + i) % capacity_;
        const auto& entry = ring_[idx];
        if (entry.event.eventType != EventType::Press) continue;

        if (entry.event.code == codes[matchIdx]) {
            if (matchIdx == 0) {
                firstMatchTime = entry.event.timestamp;
            } else {
                if (entry.event.timestamp - firstMatchTime > window) {
                    // Reset and try from current position
                    if (entry.event.code == codes[0]) {
                        matchIdx = 0;
                        firstMatchTime = entry.event.timestamp;
                    } else {
                        matchIdx = 0;
                        continue;
                    }
                }
            }
            ++matchIdx;
        } else if (entry.event.code == codes[0]) {
            matchIdx = 1;
            firstMatchTime = entry.event.timestamp;
        } else {
            matchIdx = 0;
        }
    }

    return matchIdx >= codes.size();
}

uint64_t InputBuffer::nextSequence() {
    std::lock_guard<std::mutex> lock(mutex_);
    return sequenceCounter_++;
}

uint64_t InputBuffer::makeKey(DeviceType type, uint16_t code) const {
    return (static_cast<uint64_t>(type) << 48) | code;
}

} // namespace ergo::input
