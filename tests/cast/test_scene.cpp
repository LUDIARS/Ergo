#include "gtest/gtest.h"

#include <string>
#include <vector>

#include "ergo/cast/scene.h"

using namespace ergo::cast;

namespace {

// Minimal concrete Actor for tests (mirrors tests/actor/test_actor.cpp).
struct Dummy : ergo::actor::Actor {
    explicit Dummy(std::string n) : Actor(std::move(n), nullptr) {}
};

} // namespace

// ---------------------------------------------------------------------------
// Membership: add / contains / size / remove (ptr + handle), null + dup no-op.
// ---------------------------------------------------------------------------

TEST(CastScene, AddContainsSize) {
    Dummy a("a"), b("b");
    Scene s("level1");
    EXPECT_EQ(s.name(), "level1");
    EXPECT_EQ(s.size(), 0u);

    s.add(&a);
    s.add(&b);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_TRUE(s.contains(&a));
    EXPECT_TRUE(s.contains(&b));
    EXPECT_TRUE(s.contains(a.handle()));
    EXPECT_TRUE(s.contains(b.handle()));
}

TEST(CastScene, AddNullIsNoOp) {
    Scene s("s");
    s.add(nullptr);
    EXPECT_EQ(s.size(), 0u);
}

TEST(CastScene, AddDuplicateSameHandleIsNoOp) {
    Dummy a("a");
    Scene s("s");
    s.add(&a);
    s.add(&a);            // same handle
    s.add(&a, false);     // still same handle, different flag arg
    EXPECT_EQ(s.size(), 1u);
}

TEST(CastScene, RemoveByPointer) {
    Dummy a("a"), b("b");
    Scene s("s");
    s.add(&a);
    s.add(&b);
    EXPECT_TRUE(s.remove(&a));
    EXPECT_FALSE(s.contains(&a));
    EXPECT_EQ(s.size(), 1u);
    EXPECT_FALSE(s.remove(&a));     // already gone
    EXPECT_FALSE(s.remove(nullptr));
}

TEST(CastScene, RemoveByHandle) {
    Dummy a("a"), b("b");
    Scene s("s");
    s.add(&a);
    s.add(&b);
    EXPECT_TRUE(s.remove(b.handle()));
    EXPECT_FALSE(s.contains(b.handle()));
    EXPECT_EQ(s.size(), 1u);
    EXPECT_FALSE(s.remove(b.handle()));
    EXPECT_FALSE(s.remove(ergo::actor::Handle{999999}));
}

TEST(CastScene, ActorsReturnsInsertionOrder) {
    Dummy a("a"), b("b"), c("c");
    Scene s("s");
    s.add(&a);
    s.add(&b);
    s.add(&c);
    const auto v = s.actors();
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], &a);
    EXPECT_EQ(v[1], &b);
    EXPECT_EQ(v[2], &c);
}

// ---------------------------------------------------------------------------
// Scene activate / deactivate toggles active() and per-member is_active().
// ---------------------------------------------------------------------------

TEST(CastScene, SceneActivationTogglesEffectiveState) {
    Dummy a("a"), b("b");
    Scene s("s");
    s.add(&a);
    s.add(&b);

    EXPECT_FALSE(s.active());
    EXPECT_FALSE(s.is_active(&a));
    EXPECT_FALSE(s.is_active(&b));

    s.activate();
    EXPECT_TRUE(s.active());
    EXPECT_TRUE(s.is_active(&a));
    EXPECT_TRUE(s.is_active(&b));

    s.deactivate();
    EXPECT_FALSE(s.active());
    EXPECT_FALSE(s.is_active(&a));
    EXPECT_FALSE(s.is_active(&b));
}

TEST(CastScene, IsActiveFalseForNonMember) {
    Dummy a("a"), outsider("x");
    Scene s("s");
    s.add(&a);
    s.activate();
    EXPECT_TRUE(s.is_active(&a));
    EXPECT_FALSE(s.is_active(&outsider));
    EXPECT_FALSE(s.is_active(nullptr));
}

// ---------------------------------------------------------------------------
// Per-actor activate / deactivate; is_active == scene_active && entry.active.
// ---------------------------------------------------------------------------

TEST(CastScene, PerActorFlagGatesEffectiveState) {
    Dummy a("a"), b("b");
    Scene s("s");
    s.add(&a, true);
    s.add(&b, false);     // starts per-actor-inactive
    s.activate();

    EXPECT_TRUE(s.is_active(&a));
    EXPECT_FALSE(s.is_active(&b));   // scene on but per-actor off

    s.activate(&b);
    EXPECT_TRUE(s.is_active(&b));

    s.deactivate(&a);
    EXPECT_FALSE(s.is_active(&a));
    EXPECT_TRUE(s.active());          // scene itself unchanged
}

TEST(CastScene, PerActorActivateNonMemberIsNoOp) {
    Dummy a("a"), outsider("x");
    Scene s("s");
    s.add(&a);
    s.activate();
    s.activate(&outsider);            // no-op
    s.deactivate(&outsider);          // no-op
    EXPECT_EQ(s.size(), 1u);
    EXPECT_FALSE(s.contains(&outsider));
}

// ---------------------------------------------------------------------------
// Callbacks fire on effective-state transitions only.
// ---------------------------------------------------------------------------

TEST(CastScene, CallbacksFireOnSceneTransitions) {
    Dummy a("a"), b("b");
    Scene s("s");
    s.add(&a);
    s.add(&b);

    int acts = 0, deacts = 0;
    s.on_activate([&](Actor*) { ++acts; });
    s.on_deactivate([&](Actor*) { ++deacts; });

    s.activate();          // 2 members flip false->true
    EXPECT_EQ(acts, 2);
    EXPECT_EQ(deacts, 0);

    s.activate();          // already active -> no transition
    EXPECT_EQ(acts, 2);

    s.deactivate();        // 2 members flip true->false
    EXPECT_EQ(deacts, 2);

    s.deactivate();        // already inactive -> no transition
    EXPECT_EQ(deacts, 2);
}

TEST(CastScene, CallbacksDoNotFireWhenUnchanged) {
    Dummy a("a");
    Scene s("s");
    s.add(&a, false);      // per-actor inactive
    int acts = 0, deacts = 0;
    s.on_activate([&](Actor*) { ++acts; });
    s.on_deactivate([&](Actor*) { ++deacts; });

    s.activate();          // scene on, but a is per-actor-off -> no callback
    EXPECT_EQ(acts, 0);

    s.deactivate(&a);      // already effectively inactive -> no callback
    EXPECT_EQ(deacts, 0);

    s.activate(&a);        // scene on, flag false->true -> +1 activate
    EXPECT_EQ(acts, 1);

    s.activate(&a);        // flag already true -> no callback
    EXPECT_EQ(acts, 1);
}

TEST(CastScene, MultipleCallbacksAllFire) {
    Dummy a("a");
    Scene s("s");
    s.add(&a);
    int c1 = 0, c2 = 0;
    s.on_activate([&](Actor*) { ++c1; });
    s.on_activate([&](Actor*) { ++c2; });
    s.activate();
    EXPECT_EQ(c1, 1);
    EXPECT_EQ(c2, 1);
}

TEST(CastScene, PerActorDeactivateWhileSceneOffNoCallback) {
    Dummy a("a");
    Scene s("s");
    s.add(&a, true);
    int deacts = 0;
    s.on_deactivate([&](Actor*) { ++deacts; });
    s.deactivate(&a);      // scene off -> effective state unchanged
    EXPECT_EQ(deacts, 0);
}

// ---------------------------------------------------------------------------
// Snapshot / restore
// ---------------------------------------------------------------------------

TEST(CastScene, SnapshotCapturesState) {
    Dummy a("alpha"), b("beta");
    Scene s("scn");
    s.add(&a, true);
    s.add(&b, false);
    s.activate();

    const SceneSnapshot snap = s.snapshot();
    EXPECT_EQ(snap.scene_name, "scn");
    EXPECT_TRUE(snap.scene_active);
    ASSERT_EQ(snap.entries.size(), 2u);
    EXPECT_EQ(snap.entries[0].handle, a.handle());
    EXPECT_EQ(snap.entries[0].name, "alpha");
    EXPECT_TRUE(snap.entries[0].active);
    EXPECT_EQ(snap.entries[1].handle, b.handle());
    EXPECT_EQ(snap.entries[1].name, "beta");
    EXPECT_FALSE(snap.entries[1].active);
}

TEST(CastScene, RestoreReappliesFlagsAndSceneActive) {
    Dummy a("a"), b("b");
    Scene s("s");
    s.add(&a, true);
    s.add(&b, true);
    s.activate();
    const SceneSnapshot snap = s.snapshot();   // both active, scene active

    s.deactivate();
    s.deactivate(&a);
    EXPECT_FALSE(s.active());

    s.restore(snap);
    EXPECT_TRUE(s.active());
    EXPECT_TRUE(s.is_active(&a));
    EXPECT_TRUE(s.is_active(&b));
}

TEST(CastScene, RestoreIgnoresUnknownHandlesAndKeepsMembership) {
    Dummy a("a");
    Scene s("s");
    s.add(&a, true);

    SceneSnapshot snap;
    snap.scene_name   = "s";
    snap.scene_active = true;
    snap.entries.push_back(ActorEntry{a.handle(), "a", false});
    snap.entries.push_back(ActorEntry{ergo::actor::Handle{123456}, "ghost", true});

    s.restore(snap);
    EXPECT_EQ(s.size(), 1u);            // membership unchanged
    EXPECT_TRUE(s.contains(&a));
    EXPECT_FALSE(s.contains(ergo::actor::Handle{123456}));
    EXPECT_TRUE(s.active());
    EXPECT_FALSE(s.is_active(&a));      // restored per-actor flag = false
}

TEST(CastScene, RestoreFiresCallbacksOnChangedEntries) {
    Dummy a("a"), b("b");
    Scene s("s");
    s.add(&a, true);
    s.add(&b, true);
    s.activate();
    s.deactivate(&b);
    const SceneSnapshot snap = s.snapshot();   // scene on, a on, b off

    s.deactivate();                            // everything effectively off

    int acts = 0, deacts = 0;
    s.on_activate([&](Actor*) { ++acts; });
    s.on_deactivate([&](Actor*) { ++deacts; });

    s.restore(snap);                           // a: false->true (+1), b: stays false
    EXPECT_EQ(acts, 1);
    EXPECT_EQ(deacts, 0);
    EXPECT_TRUE(s.is_active(&a));
    EXPECT_FALSE(s.is_active(&b));
}

TEST(CastScene, RestoreDoesNotAddOrRemoveMembers) {
    Dummy a("a"), b("b");
    Scene s("s");
    s.add(&a, true);   // only a is a member

    SceneSnapshot snap;
    snap.scene_name   = "s";
    snap.scene_active = false;
    snap.entries.push_back(ActorEntry{a.handle(), "a", true});
    snap.entries.push_back(ActorEntry{b.handle(), "b", true});

    s.restore(snap);
    EXPECT_EQ(s.size(), 1u);
    EXPECT_TRUE(s.contains(&a));
    EXPECT_FALSE(s.contains(&b));
}