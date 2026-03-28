#include <gtest/gtest.h>
#include "ergo/event/event_sequencer.h"
#include <string>

using namespace ergo::event;

class EventSequencerTest : public ::testing::Test {
protected:
    SequencerConfig config;

    void SetUp() override {
        config.maxProcessPerFrame = 10;
        config.maxQueueSize = 64;
    }
};

// イベントがキューに追加され順次処理されること
TEST_F(EventSequencerTest, EmitAndProcess) {
    EventSequencer seq(config);
    int count = 0;

    seq.on(1, [&](const Event&) { ++count; });
    seq.emit(1);
    seq.emit(1);
    seq.emit(1);

    EXPECT_EQ(seq.pendingCount(), 3u);
    uint32_t processed = seq.update();
    EXPECT_EQ(processed, 3u);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(seq.pendingCount(), 0u);
}

// 優先度の高いイベントが先に処理されること
TEST_F(EventSequencerTest, PriorityOrder) {
    EventSequencer seq(config);
    std::vector<EventId> order;

    seq.on(1, [&](const Event&) { order.push_back(1); });
    seq.on(2, [&](const Event&) { order.push_back(2); });
    seq.on(3, [&](const Event&) { order.push_back(3); });

    seq.emit(1, {}, Priority::Low);
    seq.emit(2, {}, Priority::High);
    seq.emit(3, {}, Priority::Normal);

    seq.update();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 2u); // High
    EXPECT_EQ(order[1], 3u); // Normal
    EXPECT_EQ(order[2], 1u); // Low
}

// リスナーが正しく登録・解除されること
TEST_F(EventSequencerTest, ListenerRegistrationAndRemoval) {
    EventSequencer seq(config);
    int count = 0;

    ListenerId lid = seq.on(1, [&](const Event&) { ++count; });
    seq.emit(1);
    seq.update();
    EXPECT_EQ(count, 1);

    EXPECT_TRUE(seq.off(lid));
    seq.emit(1);
    seq.update();
    EXPECT_EQ(count, 1); // リスナー解除済みなので増えない
}

// offAllで指定イベントの全リスナーが解除されること
TEST_F(EventSequencerTest, OffAll) {
    EventSequencer seq(config);
    int count = 0;

    seq.on(1, [&](const Event&) { ++count; });
    seq.on(1, [&](const Event&) { ++count; });

    seq.offAll(1);
    seq.emit(1);
    seq.update();
    EXPECT_EQ(count, 0);
}

// フレームあたり最大処理数を超えた場合に次フレームに持ち越されること
TEST_F(EventSequencerTest, MaxProcessPerFrame) {
    SequencerConfig cfg;
    cfg.maxProcessPerFrame = 3;
    cfg.maxQueueSize = 64;
    EventSequencer seq(cfg);

    int count = 0;
    seq.on(1, [&](const Event&) { ++count; });

    for (int i = 0; i < 7; ++i) {
        seq.emit(1);
    }

    uint32_t p1 = seq.update();
    EXPECT_EQ(p1, 3u);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(seq.pendingCount(), 4u);

    uint32_t p2 = seq.update();
    EXPECT_EQ(p2, 3u);
    EXPECT_EQ(count, 6);

    uint32_t p3 = seq.update();
    EXPECT_EQ(p3, 1u);
    EXPECT_EQ(count, 7);
}

// 遅延イベントが指定フレーム後に発行されること
TEST_F(EventSequencerTest, DelayedEvent) {
    EventSequencer seq(config);
    int count = 0;

    seq.on(1, [&](const Event&) { ++count; });
    seq.emitDelayed(1, 3); // 3フレーム後

    seq.update(); // フレーム1: まだ処理されない
    EXPECT_EQ(count, 0);

    seq.update(); // フレーム2: まだ処理されない
    EXPECT_EQ(count, 0);

    seq.update(); // フレーム3: キューに入って処理される
    EXPECT_EQ(count, 1);
}

// 遅延0はemitと同じ
TEST_F(EventSequencerTest, DelayZeroIsImmediate) {
    EventSequencer seq(config);
    int count = 0;

    seq.on(1, [&](const Event&) { ++count; });
    seq.emitDelayed(1, 0);

    seq.update();
    EXPECT_EQ(count, 1);
}

// キューが空の場合に何も処理されないこと
TEST_F(EventSequencerTest, EmptyQueueUpdate) {
    EventSequencer seq(config);
    uint32_t processed = seq.update();
    EXPECT_EQ(processed, 0u);
}

// 処理中にイベントを発行しても安全に動作すること
TEST_F(EventSequencerTest, EmitDuringDispatch) {
    EventSequencer seq(config);
    int count = 0;

    seq.on(1, [&](const Event&) {
        ++count;
        if (count == 1) {
            seq.emit(1); // 処理中に再発行
        }
    });

    seq.emit(1);
    seq.update();
    EXPECT_EQ(count, 1); // 1回目のフレームでは1回だけ

    seq.update();
    EXPECT_EQ(count, 2); // 2フレーム目で再発行分が処理される
}

// リスナーが未登録のイベントが発行されても例外が起きないこと
TEST_F(EventSequencerTest, NoListenerNoException) {
    EventSequencer seq(config);
    seq.emit(999); // リスナー未登録
    EXPECT_NO_THROW(seq.update());
}

// キューのクリアで全イベントが除去されること
TEST_F(EventSequencerTest, ClearAll) {
    EventSequencer seq(config);

    seq.emit(1);
    seq.emit(2);
    seq.emitDelayed(3, 5);

    EXPECT_EQ(seq.pendingCount(), 2u);
    EXPECT_EQ(seq.delayedCount(), 1u);

    seq.clear();
    EXPECT_EQ(seq.pendingCount(), 0u);
    EXPECT_EQ(seq.delayedCount(), 0u);
}

// ペイロードが正しく伝達されること
TEST_F(EventSequencerTest, PayloadDelivery) {
    EventSequencer seq(config);
    std::string received;

    seq.on(1, [&](const Event& e) {
        received = std::any_cast<std::string>(e.payload);
    });

    seq.emit(1, std::string("hello"));
    seq.update();
    EXPECT_EQ(received, "hello");
}

// 存在しないリスナーIDの解除がfalseを返すこと
TEST_F(EventSequencerTest, OffInvalidListener) {
    EventSequencer seq(config);
    EXPECT_FALSE(seq.off(999));
}
