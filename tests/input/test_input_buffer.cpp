#include "gtest/gtest.h"
#include "ergo/input/input_buffer.h"
#include <thread>

using namespace ergo::input;

TEST(InputBuffer, EventsBufferedInOrder) {
    InputBuffer buf(16);
    auto now = Clock::now();

    for (uint16_t i = 0; i < 5; ++i) {
        InputEvent ev{DeviceType::Keyboard, 0, EventType::Press, i, 1.0f, 0, now};
        buf.push(ev);
    }

    EXPECT_EQ(buf.size(), 5u);
    for (uint32_t i = 0; i < 5; ++i) {
        EXPECT_EQ(buf.at(i).event.code, i);
        EXPECT_EQ(buf.at(i).event.sequence, i);
    }
}

TEST(InputBuffer, OverflowOverwritesOldest) {
    InputBuffer buf(4);
    auto now = Clock::now();

    for (uint16_t i = 0; i < 6; ++i) {
        InputEvent ev{DeviceType::Keyboard, 0, EventType::Press, i, 1.0f, 0, now};
        buf.push(ev);
    }

    EXPECT_EQ(buf.size(), 4u);
    EXPECT_EQ(buf.at(0).event.code, 2);
    EXPECT_EQ(buf.at(3).event.code, 5);
}

TEST(InputBuffer, HoldDuration) {
    InputBuffer buf(16);
    auto start = Clock::now();

    InputEvent press{DeviceType::Keyboard, 0, EventType::Press,
                     static_cast<uint16_t>(KeyCode::A), 1.0f, 0, start};
    buf.push(press);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto dur = buf.holdDuration(DeviceType::Keyboard, static_cast<uint16_t>(KeyCode::A));
    EXPECT_GT(dur.count(), 0);
}

TEST(InputBuffer, HoldDurationOnRelease) {
    InputBuffer buf(16);
    auto start = Clock::now();
    auto later = start + std::chrono::milliseconds(50);

    InputEvent press{DeviceType::Keyboard, 0, EventType::Press,
                     static_cast<uint16_t>(KeyCode::A), 1.0f, 0, start};
    buf.push(press);

    InputEvent release{DeviceType::Keyboard, 0, EventType::Release,
                       static_cast<uint16_t>(KeyCode::A), 0.0f, 0, later};
    buf.push(release);

    // After release, holdDuration should return zero (key no longer held)
    auto dur = buf.holdDuration(DeviceType::Keyboard, static_cast<uint16_t>(KeyCode::A));
    EXPECT_EQ(dur.count(), 0);

    // But the entry in the buffer should have the hold duration
    EXPECT_GT(buf.at(1).holdDuration.count(), 0);
}

TEST(InputBuffer, SequenceDetectedWithinWindow) {
    InputBuffer buf(16);
    auto base = Clock::now();

    std::vector<uint16_t> seq = {1, 2, 3};
    for (size_t i = 0; i < seq.size(); ++i) {
        InputEvent ev{DeviceType::Keyboard, 0, EventType::Press, seq[i], 1.0f, 0,
                      base + std::chrono::milliseconds(i * 10)};
        buf.push(ev);
    }

    EXPECT_TRUE(buf.matchSequence(seq, std::chrono::milliseconds(100)));
}

TEST(InputBuffer, SequenceNotDetectedOutsideWindow) {
    InputBuffer buf(16);
    auto base = Clock::now();

    std::vector<uint16_t> seq = {1, 2, 3};
    for (size_t i = 0; i < seq.size(); ++i) {
        InputEvent ev{DeviceType::Keyboard, 0, EventType::Press, seq[i], 1.0f, 0,
                      base + std::chrono::milliseconds(i * 200)};
        buf.push(ev);
    }

    EXPECT_FALSE(buf.matchSequence(seq, std::chrono::milliseconds(100)));
}

TEST(InputBuffer, ClearRemovesAllEntries) {
    InputBuffer buf(16);
    auto now = Clock::now();

    InputEvent ev{DeviceType::Keyboard, 0, EventType::Press, 1, 1.0f, 0, now};
    buf.push(ev);
    EXPECT_EQ(buf.size(), 1u);

    buf.clear();
    EXPECT_EQ(buf.size(), 0u);
}
