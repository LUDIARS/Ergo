#include "ergo/blackboard/blackboard.h"

#include "gtest/gtest.h"

#include <map>
#include <memory>
#include <string>
#include <typeindex>

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

// --- By-name lookup / subscribe ---------------------------------------------

TEST_F(BlackboardTest, find_returns_typed_property) {
    Property<int> score(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("score", &score, "stage");

    EXPECT_TRUE(bb.find<int>("score") == &score);
    EXPECT_TRUE(bb.find<float>("score") == nullptr);   // type mismatch (+ warning)
    EXPECT_TRUE(bb.find<int>("missing") == nullptr);   // not registered
}

TEST_F(BlackboardTest, subscribe_by_name_fires_on_set) {
    Property<int> score(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("score", &score, "stage");

    // The consumer only knows the name and the type — not the Property.
    int last = -1;
    auto sub = bb.subscribe<int>("score",
        [&](const int& v) { last = v; },
        "hud");
    EXPECT_TRUE(sub.active());
    EXPECT_EQ(bb.subscription_count("score"), 1u);

    score.set(42);
    EXPECT_EQ(last, 42);

    sub.reset();
    score.set(7);
    EXPECT_EQ(last, 42);
    EXPECT_EQ(bb.subscription_count("score"), 0u);
}

TEST_F(BlackboardTest, subscribe_by_name_type_mismatch_is_inactive) {
    Property<int> score(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("score", &score, "stage");

    int calls = 0;
    auto sub = bb.subscribe<float>("score",
        [&](const float&) { ++calls; },
        "hud");
    EXPECT_FALSE(sub.active());
    EXPECT_EQ(bb.subscription_count("score"), 0u);
    EXPECT_EQ(score.subscriber_count(), 0u);

    score.set(1);
    EXPECT_EQ(calls, 0);
}

TEST_F(BlackboardTest, subscribe_by_name_unregistered_is_inactive) {
    auto& bb = Engine::instance();
    int calls = 0;
    // No deferred binding: registering afterwards does not revive it.
    auto sub = bb.subscribe<int>("later", [&](const int&) { ++calls; });
    EXPECT_FALSE(sub.active());

    Property<int> later(0);
    bb.register_property<int>("later", &later);
    later.set(5);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(bb.subscription_count("later"), 0u);
    EXPECT_EQ(bb.category_count(), 0u);
}

TEST_F(BlackboardTest, subscribe_by_name_null_callback_is_inactive) {
    Property<int> score(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("score", &score);

    auto sub = bb.subscribe<int>("score", std::function<void(const int&)>{});
    EXPECT_FALSE(sub.active());
    EXPECT_EQ(score.subscriber_count(), 0u);
}

TEST_F(BlackboardTest, pointer_overload_still_selected_for_nullptr) {
    // (name, nullptr, cb) must keep resolving to the pointer overload (the
    // by-name overload is not viable: a lambda is not a std::string).
    auto& bb = Engine::instance();
    auto sub = bb.subscribe<int>("p", nullptr, [](const int&) {});
    EXPECT_FALSE(sub.active());
    EXPECT_EQ(bb.subscription_count("p"), 0u);
}

TEST_F(BlackboardTest, unregister_closes_by_name_subscriptions) {
    auto owner = std::make_unique<Property<int>>(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("hp", owner.get(), "stage");

    int calls = 0;
    auto sub = bb.subscribe<int>("hp", [&](const int&) { ++calls; }, "hud");
    owner->set(1);
    EXPECT_EQ(calls, 1);

    bb.unregister("hp");
    EXPECT_FALSE(sub.active());
    EXPECT_EQ(owner->subscriber_count(), 0u);
    EXPECT_EQ(bb.subscription_count("hp"), 0u);

    owner->set(2);
    EXPECT_EQ(calls, 1);

    // The producer's Property dies before the HUD's handle: dropping the
    // handle and releasing the HUD category must not touch it again.
    owner.reset();
    sub.reset();
    bb.release("hud");
    EXPECT_EQ(bb.subscription_count("hp"), 0u);
}

TEST_F(BlackboardTest, release_owner_category_closes_by_name_subscriptions) {
    auto owner = std::make_unique<Property<int>>(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("hp", owner.get(), "stage");

    int calls = 0;
    {
        auto sub = bb.subscribe<int>("hp", [&](const int&) { ++calls; }, "hud");
        EXPECT_EQ(bb.subscription_count("hp"), 1u);

        bb.release("stage");              // owner's category, not the HUD's
        EXPECT_FALSE(sub.active());
        EXPECT_EQ(bb.registered_property_names().size(), 0u);
        EXPECT_EQ(bb.subscription_count("hp"), 0u);

        owner->set(3);
        EXPECT_EQ(calls, 0);

        owner.reset();                    // Property gone, handle still alive
    }                                     // handle destructs: no-op
    bb.release("hud");
    EXPECT_EQ(bb.subscription_count("hp"), 0u);
}

TEST_F(BlackboardTest, reregister_closes_old_by_name_subscriptions) {
    Property<int> a(0);
    Property<int> b(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("x", &a, "stage");

    int calls = 0;
    auto old_sub = bb.subscribe<int>("x", [&](const int&) { ++calls; }, "hud");

    bb.register_property<int>("x", &b, "stage");   // replaces (+ warning)
    EXPECT_FALSE(old_sub.active());
    EXPECT_EQ(a.subscriber_count(), 0u);
    a.set(1);
    b.set(1);
    EXPECT_EQ(calls, 0);

    // Re-subscribing by name follows the new registration.
    auto new_sub = bb.subscribe<int>("x", [&](const int&) { ++calls; }, "hud");
    EXPECT_TRUE(new_sub.active());
    b.set(2);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(bb.subscription_count("x"), 1u);
}

TEST_F(BlackboardTest, release_subscriber_category_keeps_registration) {
    Property<int> score(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("score", &score, "stage");

    int calls = 0;
    auto sub = bb.subscribe<int>("score", [&](const int&) { ++calls; }, "hud");
    bb.release("hud");
    EXPECT_FALSE(sub.active());
    EXPECT_EQ(score.subscriber_count(), 0u);
    EXPECT_EQ(bb.registered_property_names().size(), 1u);

    // Unregistering afterwards finds the record already closed.
    bb.unregister("score");
    EXPECT_EQ(bb.subscription_count("score"), 0u);
    score.set(1);
    EXPECT_EQ(calls, 0);
}

TEST_F(BlackboardTest, release_all_closes_by_name_subscriptions) {
    Property<int> score(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("score", &score, "stage");
    auto sub = bb.subscribe<int>("score", [](const int&) {}, "hud");

    bb.release_all();
    EXPECT_FALSE(sub.active());
    EXPECT_EQ(score.subscriber_count(), 0u);
    EXPECT_EQ(bb.subscription_count("score"), 0u);
}

TEST_F(BlackboardTest, visit_enumerates_entries) {
    Property<int>   score(3);
    Property<float> speed(1.5f);
    auto& bb = Engine::instance();
    bb.register_property<int>("score", &score, "stage");
    bb.register_property<float>("speed", &speed, "player");

    std::map<std::string, std::pair<std::type_index, const void*>> seen;
    bb.visit([&](const std::string& name, std::type_index type, const void* ptr) {
        seen.emplace(name, std::make_pair(type, ptr));
    });

    ASSERT_EQ(seen.size(), 2u);
    EXPECT_TRUE(seen.at("score").first == std::type_index(typeid(int)));
    EXPECT_TRUE(seen.at("score").second == &score);
    EXPECT_TRUE(seen.at("speed").first == std::type_index(typeid(float)));
    EXPECT_TRUE(seen.at("speed").second == &speed);

    // The erased pointer casts back once the type is matched.
    const auto* p = static_cast<const Property<int>*>(seen.at("score").second);
    EXPECT_EQ(p->get(), 3);
}

TEST_F(BlackboardTest, subscription_count_consistent_across_paths) {
    Property<int> score(0);
    auto& bb = Engine::instance();
    bb.register_property<int>("score", &score, "stage");

    auto by_ptr  = bb.subscribe<int>("score", &score, [](const int&) {}, "hud");
    auto by_name = bb.subscribe<int>("score", [](const int&) {}, "hud");
    EXPECT_EQ(bb.subscription_count("score"), 2u);
    EXPECT_EQ(score.subscriber_count(), 2u);

    // unregister closes only the by-name one: the pointer subscriber holds
    // the Property itself and stays live until its handle / category goes.
    bb.unregister("score");
    EXPECT_FALSE(by_name.active());
    EXPECT_TRUE(by_ptr.active());
    EXPECT_EQ(bb.subscription_count("score"), 1u);

    by_name.reset();                  // already closed: no double decrement
    EXPECT_EQ(bb.subscription_count("score"), 1u);

    bb.release("hud");
    EXPECT_FALSE(by_ptr.active());
    EXPECT_EQ(bb.subscription_count("score"), 0u);
    EXPECT_EQ(score.subscriber_count(), 0u);
}
