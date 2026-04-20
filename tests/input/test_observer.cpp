#include "gtest/gtest.h"
#include "ergo/input/observer.h"

using namespace ergo::input;

TEST(Observer, CallbackReceivesEvent) {
    Observer obs(DeliveryPolicy::Immediate);
    InputEvent received{};
    bool called = false;

    obs.subscribe([&](const InputEvent& ev) {
        received = ev;
        called = true;
    });

    InputEvent ev{DeviceType::Keyboard, 0, EventType::Press,
                  static_cast<uint16_t>(KeyCode::A), 1.0f, 0, Clock::now()};
    obs.notify(ev);

    EXPECT_TRUE(called);
    EXPECT_EQ(received.code, static_cast<uint16_t>(KeyCode::A));
}

TEST(Observer, UnsubscribePreventsCallback) {
    Observer obs(DeliveryPolicy::Immediate);
    bool called = false;

    auto handle = obs.subscribe([&](const InputEvent&) { called = true; });
    obs.unsubscribe(handle);

    InputEvent ev{DeviceType::Keyboard, 0, EventType::Press, 0, 1.0f, 0, Clock::now()};
    obs.notify(ev);

    EXPECT_FALSE(called);
}

TEST(Observer, DeviceIndexFilter) {
    Observer obs(DeliveryPolicy::Immediate);
    int callCount = 0;

    EventFilter filter;
    filter.deviceIndex = 1;
    obs.subscribe([&](const InputEvent&) { ++callCount; }, filter);

    InputEvent ev0{DeviceType::Mouse, 0, EventType::Press, 0, 1.0f, 0, Clock::now()};
    InputEvent ev1{DeviceType::Mouse, 1, EventType::Press, 0, 1.0f, 0, Clock::now()};
    obs.notify(ev0);
    obs.notify(ev1);

    EXPECT_EQ(callCount, 1);
}

TEST(Observer, KeyCodeFilter) {
    Observer obs(DeliveryPolicy::Immediate);
    int callCount = 0;

    EventFilter filter;
    filter.keyCode = static_cast<uint16_t>(KeyCode::Space);
    obs.subscribe([&](const InputEvent&) { ++callCount; }, filter);

    InputEvent ev1{DeviceType::Keyboard, 0, EventType::Press,
                   static_cast<uint16_t>(KeyCode::A), 1.0f, 0, Clock::now()};
    InputEvent ev2{DeviceType::Keyboard, 0, EventType::Press,
                   static_cast<uint16_t>(KeyCode::Space), 1.0f, 0, Clock::now()};
    obs.notify(ev1);
    obs.notify(ev2);

    EXPECT_EQ(callCount, 1);
}

TEST(Observer, ImmediateDeliverySameTimingAsNotify) {
    Observer obs(DeliveryPolicy::Immediate);
    bool calledDuringNotify = false;

    obs.subscribe([&](const InputEvent&) { calledDuringNotify = true; });

    InputEvent ev{DeviceType::Keyboard, 0, EventType::Press, 0, 1.0f, 0, Clock::now()};
    obs.notify(ev);

    EXPECT_TRUE(calledDuringNotify);
}

TEST(Observer, FrameSyncDeliveryOnFlush) {
    Observer obs(DeliveryPolicy::FrameSync);
    int callCount = 0;

    obs.subscribe([&](const InputEvent&) { ++callCount; });

    InputEvent ev1{DeviceType::Keyboard, 0, EventType::Press, 1, 1.0f, 0, Clock::now()};
    InputEvent ev2{DeviceType::Keyboard, 0, EventType::Press, 2, 1.0f, 0, Clock::now()};
    obs.notify(ev1);
    obs.notify(ev2);

    EXPECT_EQ(callCount, 0);

    obs.flush();
    EXPECT_EQ(callCount, 2);
}

TEST(Observer, UnsubscribeInsideCallbackIsSafe) {
    Observer obs(DeliveryPolicy::Immediate);
    SubscriptionHandle handle = InvalidHandle;
    int callCount = 0;

    handle = obs.subscribe([&](const InputEvent&) {
        ++callCount;
        obs.unsubscribe(handle);
    });

    InputEvent ev{DeviceType::Keyboard, 0, EventType::Press, 0, 1.0f, 0, Clock::now()};
    obs.notify(ev);
    EXPECT_EQ(callCount, 1);

    obs.notify(ev);
    EXPECT_EQ(callCount, 1);
}
