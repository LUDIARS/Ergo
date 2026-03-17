#pragma once

#include "ergo/input/types.h"
#include <functional>
#include <vector>
#include <mutex>
#include <optional>

namespace ergo::input {

struct EventFilter {
    std::optional<DeviceIndex> deviceIndex;
    std::optional<uint16_t> keyCode;
};

using EventCallback = std::function<void(const InputEvent&)>;

class Observer {
public:
    explicit Observer(DeliveryPolicy policy = DeliveryPolicy::Immediate);

    SubscriptionHandle subscribe(EventCallback callback, EventFilter filter = {});
    void unsubscribe(SubscriptionHandle handle);

    void notify(const InputEvent& event);

    void flush();

private:
    struct Subscription {
        SubscriptionHandle handle;
        EventCallback callback;
        EventFilter filter;
    };

    DeliveryPolicy policy_;
    std::vector<Subscription> subscriptions_;
    std::vector<InputEvent> pendingQueue_;
    SubscriptionHandle nextHandle_ = 1;
    mutable std::mutex mutex_;

    bool matchesFilter(const InputEvent& event, const EventFilter& filter) const;
};

} // namespace ergo::input
