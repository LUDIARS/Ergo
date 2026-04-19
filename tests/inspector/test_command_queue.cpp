#include "ergo/inspector/command_queue.h"
#include "gtest/gtest.h"

#include <thread>
#include <vector>

using namespace ergo::inspector;

TEST(CommandQueue, PushAndDrain) {
    CommandQueue q;
    EXPECT_EQ(q.size_approx(), 0u);

    q.push({1, Value::of_double(1.5)});
    q.push({2, Value::of_int32(7)});
    EXPECT_EQ(q.size_approx(), 2u);

    auto drained = q.drain();
    ASSERT_EQ(drained.size(), 2u);
    EXPECT_EQ(drained[0].handle, 1u);
    EXPECT_EQ(drained[0].value.d, 1.5);
    EXPECT_EQ(drained[1].handle, 2u);
    EXPECT_EQ(drained[1].value.i, 7);

    EXPECT_EQ(q.size_approx(), 0u);
    EXPECT_EQ(q.drain().size(), 0u);
}

TEST(CommandQueue, ClearEmpties) {
    CommandQueue q;
    q.push({1, Value::of_bool(true)});
    q.clear();
    EXPECT_EQ(q.size_approx(), 0u);
}

TEST(CommandQueue, MultiProducerSingleConsumer) {
    CommandQueue q;
    constexpr int N = 1000;
    constexpr int producers = 4;

    std::vector<std::thread> th;
    for (int p = 0; p < producers; ++p) {
        th.emplace_back([&q, p, N]{
            for (int i = 0; i < N; ++i) {
                q.push({static_cast<Handle>(p * N + i + 1), Value::of_int32(i)});
            }
        });
    }
    for (auto& t : th) t.join();

    auto drained = q.drain();
    EXPECT_EQ(drained.size(), static_cast<std::size_t>(producers * N));
}
