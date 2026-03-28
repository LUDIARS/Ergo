#include "ergo/event/event_queue.h"
#include <algorithm>

namespace ergo::event {

EventQueue::EventQueue(uint32_t maxSize)
    : maxSize_(maxSize)
{
    heap_.reserve(std::min(maxSize, uint32_t(256)));
}

bool EventQueue::push(const Event& event) {
    if (heap_.size() >= maxSize_) {
        return false;
    }
    heap_.push_back(event);
    heapUp(heap_.size() - 1);
    return true;
}

bool EventQueue::pop(Event& out) {
    if (heap_.empty()) {
        return false;
    }
    out = std::move(heap_[0]);
    heap_[0] = std::move(heap_.back());
    heap_.pop_back();
    if (!heap_.empty()) {
        heapDown(0);
    }
    return true;
}

std::size_t EventQueue::size() const {
    return heap_.size();
}

bool EventQueue::empty() const {
    return heap_.empty();
}

void EventQueue::clear() {
    heap_.clear();
}

uint32_t EventQueue::maxSize() const {
    return maxSize_;
}

// Max-heap: 優先度が高いものが先頭に来る
void EventQueue::heapUp(std::size_t index) {
    while (index > 0) {
        std::size_t parent = (index - 1) / 2;
        if (static_cast<uint8_t>(heap_[index].priority) >
            static_cast<uint8_t>(heap_[parent].priority)) {
            std::swap(heap_[index], heap_[parent]);
            index = parent;
        } else {
            break;
        }
    }
}

void EventQueue::heapDown(std::size_t index) {
    std::size_t sz = heap_.size();
    while (true) {
        std::size_t largest = index;
        std::size_t left = 2 * index + 1;
        std::size_t right = 2 * index + 2;

        if (left < sz &&
            static_cast<uint8_t>(heap_[left].priority) >
            static_cast<uint8_t>(heap_[largest].priority)) {
            largest = left;
        }
        if (right < sz &&
            static_cast<uint8_t>(heap_[right].priority) >
            static_cast<uint8_t>(heap_[largest].priority)) {
            largest = right;
        }
        if (largest != index) {
            std::swap(heap_[index], heap_[largest]);
            index = largest;
        } else {
            break;
        }
    }
}

} // namespace ergo::event
