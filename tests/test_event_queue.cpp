#include <gtest/gtest.h>
#include "ergo/event/event_queue.h"

using namespace ergo::event;

class EventQueueTest : public ::testing::Test {
protected:
    EventQueue queue{16};
};

// イベントがキューに追加され順次処理されること
TEST_F(EventQueueTest, PushAndPop) {
    Event e1;
    e1.id = 1;
    e1.priority = Priority::Normal;

    Event e2;
    e2.id = 2;
    e2.priority = Priority::Normal;

    EXPECT_TRUE(queue.push(e1));
    EXPECT_TRUE(queue.push(e2));
    EXPECT_EQ(queue.size(), 2u);

    Event out;
    EXPECT_TRUE(queue.pop(out));
    EXPECT_TRUE(queue.pop(out));
    EXPECT_TRUE(queue.empty());
}

// 優先度の高いイベントが先に処理されること
TEST_F(EventQueueTest, PriorityOrdering) {
    Event low;
    low.id = 1;
    low.priority = Priority::Low;

    Event normal;
    normal.id = 2;
    normal.priority = Priority::Normal;

    Event high;
    high.id = 3;
    high.priority = Priority::High;

    queue.push(low);
    queue.push(normal);
    queue.push(high);

    Event out;
    EXPECT_TRUE(queue.pop(out));
    EXPECT_EQ(out.id, 3u); // High

    EXPECT_TRUE(queue.pop(out));
    EXPECT_EQ(out.id, 2u); // Normal

    EXPECT_TRUE(queue.pop(out));
    EXPECT_EQ(out.id, 1u); // Low
}

// キューが空の場合にpopがfalseを返すこと
TEST_F(EventQueueTest, PopFromEmpty) {
    Event out;
    EXPECT_FALSE(queue.pop(out));
    EXPECT_TRUE(queue.empty());
}

// キューのクリアで全イベントが除去されること
TEST_F(EventQueueTest, Clear) {
    Event e;
    e.id = 1;
    queue.push(e);
    queue.push(e);
    queue.push(e);

    EXPECT_EQ(queue.size(), 3u);
    queue.clear();
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0u);
}

// キュー最大サイズ超過時にイベントが破棄されること
TEST_F(EventQueueTest, MaxSizeOverflow) {
    EventQueue smallQueue(3);

    Event e;
    e.id = 1;

    EXPECT_TRUE(smallQueue.push(e));
    EXPECT_TRUE(smallQueue.push(e));
    EXPECT_TRUE(smallQueue.push(e));
    EXPECT_FALSE(smallQueue.push(e)); // 4つ目は失敗
    EXPECT_EQ(smallQueue.size(), 3u);
}

// maxSizeが正しく返ること
TEST_F(EventQueueTest, MaxSize) {
    EXPECT_EQ(queue.maxSize(), 16u);
}
