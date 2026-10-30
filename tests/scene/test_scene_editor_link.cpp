// ergo_scene_link — hermetic tests (no socket). Outgoing frames are captured
// through set_transport(); incoming patches are injected through receive().

#include "gtest/gtest.h"

#include "ergo/common/json_min.h"
#include "ergo/scene/editor_link.h"
#include "ergo/scene/scene.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace ergo::scene;
namespace jsonm = ergo::common::jsonm;

namespace {

std::unique_ptr<Scene> make_scene(const std::string& id) {
    auto s = Scene::load_json(R"JSON({
      "id": "ID",
      "actors": [
        { "id": "crate", "name": "Crate", "transform": { "pos": [1,2,3] } },
        { "id": "lamp",  "name": "Lamp",  "parent": "crate" }
      ]
    })JSON");
    s->set_id(id);
    return s;
}

struct Wire {
    std::vector<std::string> frames;
    EditorLink::SendHook hook() {
        return [this](const std::string& f) { frames.push_back(f); };
    }
    jsonm::JsonValue at(std::size_t i) const {
        jsonm::JsonValue v;
        if (i < frames.size()) jsonm::parse(frames[i], v);
        return v;
    }
    static std::string op(const jsonm::JsonValue& v) {
        const jsonm::JsonValue* o = v.find("op");
        return o ? o->as_string() : std::string();
    }
    static std::string str(const jsonm::JsonValue& v, const char* key) {
        const jsonm::JsonValue* o = v.find(key);
        return o ? o->as_string() : std::string();
    }
    /// The Scene carried by a `scene` frame.
    static std::unique_ptr<Scene> scene_of(const jsonm::JsonValue& v) {
        return Scene::load_json(str(v, "json"));
    }
};

std::string patch(const std::string& scene, const std::string& actor,
                  const std::string& field, const std::string& value_json) {
    return R"({"op":"patch","scene_id":")" + scene + R"(","actor_id":")" + actor +
           R"(","field":")" + field + R"(","value":)" + value_json + "}";
}

} // namespace

TEST(SceneEditorLink, OpenSendsHelloThenPublishedScenes) {
    EditorLink link;
    link.set_app_name("lookdev");
    auto a = make_scene("a");
    auto b = make_scene("b");
    EXPECT_TRUE(link.publish(*a));   // not connected yet: remembered only
    EXPECT_TRUE(link.publish(*b));

    Wire wire;
    link.set_transport(wire.hook());
    EXPECT_TRUE(link.is_connected());
    ASSERT_EQ(wire.frames.size(), 3u);
    EXPECT_EQ(Wire::op(wire.at(0)), "hello");
    EXPECT_EQ(Wire::str(wire.at(0), "role"), "engine");
    EXPECT_EQ(Wire::str(wire.at(0), "app"), "lookdev");
    EXPECT_EQ(Wire::op(wire.at(1)), "scene");
    EXPECT_EQ(Wire::str(wire.at(1), "id"), "a");
    EXPECT_EQ(Wire::str(wire.at(2), "id"), "b");

    // A reconnect (new transport) announces everything again.
    Wire again;
    link.set_transport(again.hook());
    ASSERT_EQ(again.frames.size(), 3u);
    EXPECT_EQ(Wire::op(again.at(0)), "hello");
}

TEST(SceneEditorLink, PublishCarriesSceneJson) {
    EditorLink link;
    Wire wire;
    link.set_transport(wire.hook());
    wire.frames.clear();

    auto a = make_scene("a");
    ASSERT_TRUE(link.publish(*a));
    ASSERT_EQ(wire.frames.size(), 1u);
    auto sent = Wire::scene_of(wire.at(0));
    ASSERT_TRUE(sent != nullptr);
    EXPECT_EQ(sent->id(), "a");
    EXPECT_EQ(sent->actor_count(), 2u);
    EXPECT_FLOAT_EQ(sent->find_actor("crate")->transform.position.z, 3.0f);

    Scene unnamed;
    EXPECT_FALSE(link.publish(unnamed));   // no id, nothing sent
    EXPECT_EQ(wire.frames.size(), 1u);
}

TEST(SceneEditorLink, PatchIsQueuedAndAppliedOnPoll) {
    EditorLink link;
    Wire wire;
    link.set_transport(wire.hook());
    auto a = make_scene("a");
    link.publish(*a);
    wire.frames.clear();

    EXPECT_TRUE(link.receive(patch("a", "crate", "transform.pos", "[7,8,9]")));
    EXPECT_TRUE(link.receive(patch("a", "crate", "name", R"("Box")")));
    // Nothing changes until the host polls on its own thread.
    EXPECT_FLOAT_EQ(a->find_actor("crate")->transform.position.x, 1.0f);
    EXPECT_TRUE(wire.frames.empty());

    EXPECT_EQ(link.poll(*a), 2u);
    EXPECT_FLOAT_EQ(a->find_actor("crate")->transform.position.x, 7.0f);
    EXPECT_EQ(a->find_actor("crate")->name, "Box");

    // One re-publish per touched scene, carrying the new state.
    ASSERT_EQ(wire.frames.size(), 1u);
    EXPECT_EQ(Wire::op(wire.at(0)), "scene");
    auto sent = Wire::scene_of(wire.at(0));
    ASSERT_TRUE(sent != nullptr);
    EXPECT_EQ(sent->find_actor("crate")->name, "Box");

    EXPECT_EQ(link.poll(*a), 0u);   // queue drained
    EXPECT_EQ(wire.frames.size(), 1u);
}

TEST(SceneEditorLink, RejectedPatchStillRepublishes) {
    EditorLink link;
    Wire wire;
    link.set_transport(wire.hook());
    auto a = make_scene("a");
    link.publish(*a);
    wire.frames.clear();

    // Legacy Euler path and a parent cycle: both refused by apply_patch.
    link.receive(patch("a", "crate", "transform.rot.xyz", "[0,90,0]"));
    link.receive(patch("a", "crate", "parent", R"("lamp")"));
    EXPECT_EQ(link.poll(*a), 0u);
    EXPECT_EQ(a->find_actor("crate")->parent, "");

    // The UI already shows the edited value; the re-publish snaps it back.
    ASSERT_EQ(wire.frames.size(), 1u);
    auto sent = Wire::scene_of(wire.at(0));
    ASSERT_TRUE(sent != nullptr);
    EXPECT_EQ(sent->find_actor("crate")->parent, "");
}

TEST(SceneEditorLink, PollGraphRoutesBySceneId) {
    EditorLink link;
    Wire wire;
    link.set_transport(wire.hook());
    SceneGraph graph;
    graph.mount(make_scene("a"));
    graph.mount(make_scene("b"));
    link.publish(*graph.find_scene("a"));
    link.publish(*graph.find_scene("b"));
    auto loose = make_scene("loose");      // published but not mounted
    link.publish(*loose);
    wire.frames.clear();

    link.receive(patch("b", "lamp", "name", R"("Bulb")"));
    link.receive(patch("loose", "lamp", "name", R"("Lost")"));
    EXPECT_EQ(link.poll(graph), 1u);
    EXPECT_EQ(graph.find_scene("b")->find_actor("lamp")->name, "Bulb");
    EXPECT_EQ(graph.find_scene("a")->find_actor("lamp")->name, "Lamp");

    // Only "b" was touched; the patch for the unmounted scene is discarded.
    ASSERT_EQ(wire.frames.size(), 1u);
    EXPECT_EQ(Wire::str(wire.at(0), "id"), "b");
    EXPECT_EQ(link.poll(*loose), 0u);
    EXPECT_EQ(loose->find_actor("lamp")->name, "Lamp");
}

TEST(SceneEditorLink, PollSceneLeavesOtherScenesQueued) {
    EditorLink link;
    Wire wire;
    link.set_transport(wire.hook());
    auto a = make_scene("a");
    auto b = make_scene("b");
    link.publish(*a);
    link.publish(*b);

    link.receive(patch("b", "crate", "name", R"("B1")"));
    link.receive(patch("a", "crate", "name", R"("A1")"));
    link.receive(patch("b", "crate", "visual.kind", R"("mesh")"));
    EXPECT_EQ(link.poll(*a), 1u);
    EXPECT_EQ(a->find_actor("crate")->name, "A1");
    EXPECT_EQ(b->find_actor("crate")->name, "Crate");
    EXPECT_EQ(link.poll(*b), 2u);
    EXPECT_EQ(b->find_actor("crate")->name, "B1");
    EXPECT_EQ(b->find_actor("crate")->visual.kind, "mesh");
}

TEST(SceneEditorLink, UnpublishSendsRemoveAndDropsPatches) {
    EditorLink link;
    Wire wire;
    link.set_transport(wire.hook());
    auto a = make_scene("a");
    link.publish(*a);
    link.receive(patch("a", "crate", "name", R"("Gone")"));
    wire.frames.clear();

    EXPECT_TRUE(link.unpublish("a"));
    ASSERT_EQ(wire.frames.size(), 1u);
    EXPECT_EQ(Wire::op(wire.at(0)), "scene_remove");
    EXPECT_EQ(Wire::str(wire.at(0), "id"), "a");
    EXPECT_FALSE(link.unpublish("a"));

    EXPECT_EQ(link.poll(*a), 0u);
    EXPECT_EQ(a->find_actor("crate")->name, "Crate");
    // Late patches for it are ignored, and it is not re-announced.
    EXPECT_FALSE(link.receive(patch("a", "crate", "name", R"("Late")")));
    Wire again;
    link.set_transport(again.hook());
    ASSERT_EQ(again.frames.size(), 1u);
    EXPECT_EQ(Wire::op(again.at(0)), "hello");
}

TEST(SceneEditorLink, PatchRacingUnpublishIsNeverApplied) {
    // receive() on another thread (the socket worker) races unpublish(): a
    // patch that passed the "is it published" check just before the erase
    // must not be queued after unpublish() discarded the scene's patches.
    auto a = make_scene("a");
    EditorLink link;
    link.set_transport([](const std::string&) {});
    const std::string frame = patch("a", "crate", "name", R"("x")");

    std::atomic<bool> quit{false};
    std::atomic<unsigned long long> done{0};
    std::thread worker([&] {
        while (!quit.load()) {
            link.receive(frame);
            done.fetch_add(1);
        }
    });
    auto wait_receives = [&](unsigned long long n) {
        const unsigned long long target = done.load() + n;
        while (done.load() < target) std::this_thread::yield();
    };

    std::size_t stale = 0;
    for (int i = 0; i < 2000; ++i) {
        link.publish(*a);
        wait_receives(static_cast<unsigned long long>(i % 3));
        link.poll(*a);          // patches that arrived while published
        link.unpublish("a");    // discards the scene's queued patches
        wait_receives(2);       // any receive() in flight at unpublish has finished
        stale += link.poll(*a);
    }
    quit = true;
    worker.join();
    EXPECT_EQ(stale, 0u);
}

TEST(SceneEditorLink, IgnoresNonPatchAndMalformedFrames) {
    EditorLink link;
    Wire wire;
    link.set_transport(wire.hook());
    auto a = make_scene("a");
    link.publish(*a);

    EXPECT_FALSE(link.receive("not json"));
    EXPECT_FALSE(link.receive(R"({"op":"scenes","scenes":[]})"));
    EXPECT_FALSE(link.receive(R"({"op":"patch","scene_id":"a","actor_id":"crate","field":"name"})"));  // no value
    EXPECT_FALSE(link.receive(R"({"op":"patch","scene_id":"a","actor_id":"","field":"name","value":"x"})"));
    EXPECT_FALSE(link.receive(patch("other", "crate", "name", R"("x")")));  // not published here
    EXPECT_EQ(link.poll(*a), 0u);
}

TEST(SceneEditorLink, ConnectRefusedWhileTransportInstalled) {
    EditorLink link;
    Wire wire;
    link.set_transport(wire.hook());
    EXPECT_FALSE(link.connect("127.0.0.1", 5170));
    link.set_transport(nullptr);
    EXPECT_FALSE(link.is_connected());
    EXPECT_FALSE(link.connect("", 5170));
    EXPECT_FALSE(link.connect("127.0.0.1", 0));
    link.disconnect();
}
