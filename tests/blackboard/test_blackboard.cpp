#include "ergo/blackboard/blackboard.h"

#include "gtest/gtest.h"

#include <string>

using ergo::blackboard::Engine;
using ergo::blackboard::Property;
using ergo::blackboard::Subscription;

namespace {

class BlackboardTest : public ::testing::Test {
protected:
    void SetUp() override    { Engine::instance().release_all(); }
    void TearDown() override { Engine::instance().release_all(); }
};

// Type without operator== — used to verify always-notify fallback.
struct NoEq {
    int x = 0;
};

} // namespace

// --- Property ----------------------------------------------------------------

TEST_F(BlackboardTest, property_get_set_basic) {
    Property<int> p(10);
    EXPECT_EQ(p.get(), 10);
    p.set(20);
    EXPECT_EQ(p.get(), 20);
}

TEST_F(BlackboardTest, property_distinct_skip_when_eq_supported) {
    Property<int> p(5);
    int calls = 0;
    auto tok = p.subscribe([&](const int&) { ++calls; });

    p.set(5);    // same value -> skip
    p.set(7);    // change   -> notify
    p.set(7);    // same     -> skip
    p.set(9);    // change   -> notify

    EXPECT_EQ(calls, 2);
    p.unsubscribe(tok);
}

TEST_F(BlackboardTest, property_always_notify_when_no_eq) {
    Property<NoEq> p;
    int calls = 0;
    auto tok = p.subscribe([&](const NoEq&) { ++calls; });

    p.set(NoEq{1});
    p.set(NoEq{1});   // operator== absent -> still notifies
    p.set(NoEq{2});

    EXPECT_EQ(calls, 3);
    p.unsubscribe(tok);
}

// --- Engine register / subscribe / unsubscribe -------------------------------

TEST_F(BlackboardTest, register_and_enumerate) {
    Property<int> score(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("score", &score, "stage");

    auto names = bb.registered_property_names();
    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "score");
}

TEST_F(BlackboardTest, subscribe_fires_on_change) {
    Property<int> score(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("score", &score, "stage");

    int last = -1;
    auto sub = bb.subscribe<int>("score", &score,
        [&](const int& v) { last = v; },
        "stage");

    score.set(42);
    EXPECT_EQ(last, 42);

    score.set(7);
    EXPECT_EQ(last, 7);
    EXPECT_EQ(bb.subscription_count("score"), 1u);
}

TEST_F(BlackboardTest, subscription_destruct_unsubscribes) {
    Property<int> p(0);
    auto& bb = Engine::instance();
    int calls = 0;
    {
        auto sub = bb.subscribe<int>("p", &p,
            [&](const int&) { ++calls; });
        p.set(1);
        EXPECT_EQ(calls, 1);
    } // sub goes out of scope
    p.set(2);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(bb.subscription_count("p"), 0u);
}

TEST_F(BlackboardTest, release_category_cleans_subscriptions) {
    Property<int> p(0);
    auto& bb = Engine::instance();
    int calls = 0;
    auto sub = bb.subscribe<int>("p", &p,
        [&](const int&) { ++calls; },
        "stage");

    p.set(1);
    EXPECT_EQ(calls, 1);

    bb.release("stage");
    p.set(2);
    EXPECT_EQ(calls, 1);             // no further notifications
    EXPECT_FALSE(sub.active());      // RAII handle now inert
}

TEST_F(BlackboardTest, sub_destructor_after_release_is_no_op) {
    Property<int> p(0);
    auto& bb = Engine::instance();
    {
        auto sub = bb.subscribe<int>("p", &p,
            [](const int&) {},
            "stage");
        bb.release("stage");
        // sub falls out of scope here; should not double-cleanup or crash.
    }
    EXPECT_EQ(bb.subscription_count("p"), 0u);
}

TEST_F(BlackboardTest, release_drops_registration_too) {
    Property<int> p(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("p", &p, "stage");
    EXPECT_EQ(bb.registered_property_names().size(), 1u);

    bb.release("stage");
    EXPECT_EQ(bb.registered_property_names().size(), 0u);
}

TEST_F(BlackboardTest, default_category_for_empty_string) {
    Property<int> p(0);
    auto& bb = Engine::instance();
    int calls = 0;
    auto sub = bb.subscribe<int>("p", &p,
        [&](const int&) { ++calls; });   // no category -> "Default"

    p.set(1);
    EXPECT_EQ(calls, 1);

    bb.release("");                       // empty -> "Default"
    p.set(2);
    EXPECT_EQ(calls, 1);
}

TEST_F(BlackboardTest, unregister_by_name) {
    Property<int> p(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("p", &p, "stage");
    bb.unregister("p");
    EXPECT_EQ(bb.registered_property_names().size(), 0u);
}

TEST_F(BlackboardTest, duplicate_register_replaces_with_warning) {
    Property<int> a(1);
    Property<int> b(2);
    auto& bb = Engine::instance();
    bb.register_property<int>("x", &a, "first");
    // No assertion on warning text (stderr); just ensure it doesn't throw
    // and the new entry replaces the old.
    bb.register_property<int>("x", &b, "second");
    EXPECT_EQ(bb.registered_property_names().size(), 1u);
}

TEST_F(BlackboardTest, nullptr_register_is_noop) {
    auto& bb = Engine::instance();
    bb.register_property<int>("nope", nullptr);
    EXPECT_EQ(bb.registered_property_names().size(), 0u);
}

TEST_F(BlackboardTest, subscription_count_tracks_multiple_subscribers) {
    Property<int> p(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("p", &p);

    auto a = bb.subscribe<int>("p", &p, [](const int&) {});
    auto b = bb.subscribe<int>("p", &p, [](const int&) {});
    auto c = bb.subscribe<int>("p", &p, [](const int&) {});
    EXPECT_EQ(bb.subscription_count("p"), 3u);

    a.reset();
    EXPECT_EQ(bb.subscription_count("p"), 2u);

    b.reset();
    c.reset();
    EXPECT_EQ(bb.subscription_count("p"), 0u);
}

TEST_F(BlackboardTest, release_all_resets_state) {
    Property<int> p(0), q(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("p", &p, "a");
    bb.register_property<int>("q", &q, "b");
    auto sub = bb.subscribe<int>("p", &p, [](const int&) {}, "a");

    bb.release_all();
    EXPECT_EQ(bb.registered_property_names().size(), 0u);
    EXPECT_EQ(bb.subscription_count("p"), 0u);
    EXPECT_EQ(bb.category_count(), 0u);
}

TEST_F(BlackboardTest, debug_info_lists_registered_and_categories) {
    Property<int> p(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("score", &p, "stage");
    auto sub = bb.subscribe<int>("score", &p, [](const int&) {}, "stage");

    auto info = bb.debug_info();
    EXPECT_NE(info.find("score"), std::string::npos);
    EXPECT_NE(info.find("stage"), std::string::npos);
}

TEST_F(BlackboardTest, macro_register) {
    Property<float> hp(100.0f);
    BLACKBOARD_REGISTER("health", hp, "player");
    auto& bb = Engine::instance();
    // Bind the vector to a local so its element references don't dangle
    // (the mini-gtest EXPECT_EQ macro binds with auto&& which doesn't
    // extend the lifetime of the vector returned by value).
    auto names = bb.registered_property_names();
    EXPECT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "health");
}
