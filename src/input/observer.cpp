#include "ergo/input/observer.h"
#include <algorithm>

namespace ergo::input {

Observer::Observer(DeliveryPolicy policy)
    : policy_(policy) {}

SubscriptionHandle Observer::subscribe(EventCallback callback, EventFilter filter) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto handle = nextHandle_++;
    subscriptions_.push_back({handle, std::move(callback), filter});
    return handle;
}

void Observer::unsubscribe(SubscriptionHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                        [handle](const Subscription& s) { return s.handle == handle; }),
        subscriptions_.end());
}

void Observer::notify(const InputEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (policy_ == DeliveryPolicy::Immediate) {
        // Copy subscriptions to allow safe unsubscribe during callback
        auto subs = subscriptions_;
        mutex_.unlock();
        for (const auto& sub : subs) {
            if (matchesFilter(event, sub.filter)) {
                sub.callback(event);
            }
        }
        mutex_.lock();
    } else {
        pendingQueue_.push_back(event);
    }
}

void Observer::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto events = std::move(pendingQueue_);
    pendingQueue_.clear();
    auto subs = subscriptions_;
    mutex_.unlock();

    for (const auto& event : events) {
        for (const auto& sub : subs) {
            if (matchesFilter(event, sub.filter)) {
                sub.callback(event);
            }
        }
    }

    mutex_.lock();
}

bool Observer::matchesFilter(const InputEvent& event, const EventFilter& filter) const {
    if (filter.deviceIndex && *filter.deviceIndex != event.deviceIndex) return false;
    if (filter.keyCode && *filter.keyCode != event.code) return false;
    return true;
}

} // namespace ergo::input
