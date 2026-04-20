#include "gtest/gtest.h"
#include "ergo/input/double_buffer.h"
#include <thread>

using namespace ergo::input;

struct TestState {
    int value = 0;
};

TEST(DoubleBuffer, WriteDoesNotAffectRead) {
    DoubleBuffer<TestState> buf;
    buf.writableRef().value = 42;
    EXPECT_EQ(buf.read().value, 0);
}

TEST(DoubleBuffer, SwapMakesWriteVisibleToRead) {
    DoubleBuffer<TestState> buf;
    buf.writableRef().value = 42;
    buf.swap();
    EXPECT_EQ(buf.read().value, 42);
}

TEST(DoubleBuffer, WriteAfterSwapDoesNotAffectRead) {
    DoubleBuffer<TestState> buf;
    buf.writableRef().value = 42;
    buf.swap();
    buf.writableRef().value = 99;
    EXPECT_EQ(buf.read().value, 42);
}

TEST(DoubleBuffer, ConcurrentAccessNoCorruption) {
    DoubleBuffer<TestState> buf;
    constexpr int iterations = 10000;
    std::atomic<bool> done{false};

    std::thread writer([&] {
        for (int i = 0; i < iterations; ++i) {
            buf.writableRef().value = i;
            buf.swap();
        }
        done.store(true);
    });

    std::thread reader([&] {
        while (!done.load()) {
            auto val = buf.read().value;
            EXPECT_GE(val, 0);
            EXPECT_LT(val, iterations);
        }
    });

    writer.join();
    reader.join();
}
