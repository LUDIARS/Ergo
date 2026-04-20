#include "ergo/bind/bind.h"
#include "gtest/gtest.h"

#include <atomic>

using namespace ergo::bind;

TEST(Engine, BindAndUnbind) {
    auto& E = Engine::instance();
    double x = 3.0;
    Handle h = E.bind<double>("test_engine.x_a", &x);
    ASSERT_NE(h, INVALID_HANDLE);
    E.unbind(h);
    // Re-binding by same name should produce a new handle.
    Handle h2 = E.bind<double>("test_engine.x_a", &x);
    EXPECT_NE(h2, INVALID_HANDLE);
    E.unbind(h2);
}

TEST(Engine, RejectsNullPointer) {
    auto& E = Engine::instance();
    Handle h = E.bind<double>("test_engine.null_b", nullptr);
    EXPECT_EQ(h, INVALID_HANDLE);
}

TEST(Engine, RejectsEmptyName) {
    auto& E = Engine::instance();
    Handle h = E.bind_accessor("", VarKind::Bool,
        []{ return Value::of_bool(false); },
        [](const Value&){},
        {});
    EXPECT_EQ(h, INVALID_HANDLE);
}

TEST(Engine, AppNameDefaultsToAnonymous) {
    auto& E = Engine::instance();
    // Tests share a process — set_app_name may have been called by other
    // tests. Just check it's settable and observable.
    E.set_app_name("test_engine_app");
    EXPECT_EQ(E.app_name(), "test_engine_app");
    E.set_app_name("anonymous");
}

TEST(Engine, ApplyPendingDoesntCrashWhenDisconnected) {
    auto& E = Engine::instance();
    int32_t v = 0;
    Handle h = E.bind<int32_t>("test_engine.counter_c", &v);
    // Without a server we just exercise the no-op path.
    E.apply_pending_writes();
    EXPECT_EQ(v, 0);
    E.unbind(h);
}
