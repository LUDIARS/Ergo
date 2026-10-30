#include "gtest/gtest.h"

#include "ergo/scene/scene.h"

#include <memory>
#include <string>

using namespace ergo::scene;

namespace {

const char* patch_scene_json() {
    return R"JSON({
  "id": "stage",
  "actors": [
    { "id": "root", "name": "Root", "type": "Actor", "parent": null },
    { "id": "arm", "type": "Actor", "parent": "root" },
    { "id": "hand", "type": "Actor", "parent": "arm" },
    {
      "id": "crate",
      "name": "Crate",
      "type": "PropActor",
      "parent": "root",
      "transform": { "pos": [1,2,3], "rot": [0,0,0,1], "scale": [1,1,1] },
      "visual": { "kind": "mesh", "ref": "assets/crate.mesh", "material": "assets/crate.mat" },
      "vars": [
        { "name": "health", "type": "int", "value": 100 },
        { "name": "tags", "type": "json", "value": ["a"] }
      ]
    }
  ]
})JSON";
}

std::unique_ptr<Scene> load() {
    auto s = Scene::load_json(patch_scene_json());
    return s;
}

/// apply_patch, then serialize and reload: the edit must survive the file
/// format, not just live in memory.
std::unique_ptr<Scene> round_trip(const Scene& s) {
    return Scene::load_json(s.to_json());
}

const VarSpec* find_var(const ActorSpec& a, const std::string& name) {
    for (const auto& v : a.vars) {
        if (v.name == name) return &v;
    }
    return nullptr;
}

} // namespace

TEST(ScenePatch, StringFieldsRoundTrip) {
    auto s = load();
    ASSERT_TRUE(s != nullptr);
    EXPECT_TRUE(s->apply_patch("crate", "name", R"("Big Crate")"));
    EXPECT_TRUE(s->apply_patch("crate", "type", R"("DynamicProp")"));
    EXPECT_TRUE(s->apply_patch("crate", "visual.kind", R"("sprite")"));
    EXPECT_TRUE(s->apply_patch("crate", "visual.ref", R"("assets/crate.png")"));
    EXPECT_TRUE(s->apply_patch("crate", "visual.material", R"("")"));

    auto back = round_trip(*s);
    ASSERT_TRUE(back != nullptr);
    const ActorSpec* a = back->find_actor("crate");
    ASSERT_TRUE(a != nullptr);
    EXPECT_EQ(a->name, "Big Crate");
    EXPECT_EQ(a->type, "DynamicProp");
    EXPECT_EQ(a->visual.kind, "sprite");
    EXPECT_EQ(a->visual.ref, "assets/crate.png");
    EXPECT_EQ(a->visual.material, "");
}

TEST(ScenePatch, TransformPosAndScaleRoundTrip) {
    auto s = load();
    ASSERT_TRUE(s != nullptr);
    EXPECT_TRUE(s->apply_patch("crate", "transform.pos", "[4.5, -2, 0.25]"));
    EXPECT_TRUE(s->apply_patch("crate", "transform.scale", "[2, 3, 4]"));

    auto back = round_trip(*s);
    ASSERT_TRUE(back != nullptr);
    const ActorSpec* a = back->find_actor("crate");
    ASSERT_TRUE(a != nullptr);
    EXPECT_FLOAT_EQ(a->transform.position.x, 4.5f);
    EXPECT_FLOAT_EQ(a->transform.position.y, -2.0f);
    EXPECT_FLOAT_EQ(a->transform.position.z, 0.25f);
    EXPECT_FLOAT_EQ(a->transform.scale.x, 2.0f);
    EXPECT_FLOAT_EQ(a->transform.scale.y, 3.0f);
    EXPECT_FLOAT_EQ(a->transform.scale.z, 4.0f);
}

TEST(ScenePatch, TransformRotIsNormalizedAndRoundTrips) {
    auto s = load();
    ASSERT_TRUE(s != nullptr);
    // 90 degrees about Y, deliberately not unit length.
    EXPECT_TRUE(s->apply_patch("crate", "transform.rot", "[0, 2, 0, 2]"));

    auto back = round_trip(*s);
    ASSERT_TRUE(back != nullptr);
    const Quat q = back->find_actor("crate")->transform.rotation;
    const float h = 0.70710678f;
    EXPECT_NEAR(q.x, 0.0f, 1e-5);
    EXPECT_NEAR(q.y, h, 1e-5);
    EXPECT_NEAR(q.z, 0.0f, 1e-5);
    EXPECT_NEAR(q.w, h, 1e-5);
}

TEST(ScenePatch, VarValueRoundTripsAnyJson) {
    auto s = load();
    ASSERT_TRUE(s != nullptr);
    EXPECT_TRUE(s->apply_patch("crate", "vars/health/value", "75"));
    EXPECT_TRUE(s->apply_patch("crate", "vars/tags/value", R"({"k":[1,true,"x"]})"));

    auto back = round_trip(*s);
    ASSERT_TRUE(back != nullptr);
    const ActorSpec* a = back->find_actor("crate");
    ASSERT_TRUE(a != nullptr);
    const VarSpec* health = find_var(*a, "health");
    const VarSpec* tags   = find_var(*a, "tags");
    ASSERT_TRUE(health != nullptr);
    ASSERT_TRUE(tags != nullptr);
    EXPECT_EQ(health->value, "75");
    EXPECT_EQ(health->type, "int");  // the declared type is not touched
    EXPECT_EQ(tags->value, R"({"k":[1,true,"x"]})");
}

TEST(ScenePatch, ParentRoundTripsAndClears) {
    auto s = load();
    ASSERT_TRUE(s != nullptr);
    EXPECT_TRUE(s->apply_patch("crate", "parent", R"("hand")"));
    auto back = round_trip(*s);
    ASSERT_TRUE(back != nullptr);
    EXPECT_EQ(back->find_actor("crate")->parent, "hand");

    EXPECT_TRUE(s->apply_patch("crate", "parent", R"("")"));
    EXPECT_EQ(s->find_actor("crate")->parent, "");
    EXPECT_TRUE(s->apply_patch("arm", "parent", "null"));
    back = round_trip(*s);
    ASSERT_TRUE(back != nullptr);
    EXPECT_EQ(back->find_actor("crate")->parent, "");
    EXPECT_EQ(back->find_actor("arm")->parent, "");
}

TEST(ScenePatch, ParentRejectsSelfMissingAndCycles) {
    auto s = load();
    ASSERT_TRUE(s != nullptr);
    EXPECT_FALSE(s->apply_patch("arm", "parent", R"("arm")"));      // itself
    EXPECT_FALSE(s->apply_patch("arm", "parent", R"("ghost")"));    // not in this scene
    EXPECT_FALSE(s->apply_patch("root", "parent", R"("hand")"));    // hand -> arm -> root
    EXPECT_FALSE(s->apply_patch("arm", "parent", "42"));            // wrong type
    EXPECT_EQ(s->find_actor("arm")->parent, "root");
    EXPECT_EQ(s->find_actor("root")->parent, "");
}

TEST(ScenePatch, RejectsUnknownActorAndField) {
    auto s = load();
    ASSERT_TRUE(s != nullptr);
    const std::string before = s->to_json();
    EXPECT_FALSE(s->apply_patch("ghost", "name", R"("x")"));
    EXPECT_FALSE(s->apply_patch("crate", "colour", R"("red")"));
    EXPECT_FALSE(s->apply_patch("crate", "transform", "{}"));
    EXPECT_FALSE(s->apply_patch("crate", "transform.rot.xyz", "[0, 90, 0]"));  // legacy path
    EXPECT_FALSE(s->apply_patch("crate", "vars/ghost/value", "1"));            // no such var
    EXPECT_FALSE(s->apply_patch("crate", "vars//value", "1"));
    EXPECT_FALSE(s->apply_patch("crate", "vars/health", "1"));
    EXPECT_FALSE(s->apply_patch("crate", "id", R"("renamed")"));               // ids are fixed
    EXPECT_EQ(s->to_json(), before);
}

TEST(ScenePatch, RejectsWrongJsonTypes) {
    auto s = load();
    ASSERT_TRUE(s != nullptr);
    const std::string before = s->to_json();
    EXPECT_FALSE(s->apply_patch("crate", "name", "42"));
    EXPECT_FALSE(s->apply_patch("crate", "name", "Crate"));                  // not JSON
    EXPECT_FALSE(s->apply_patch("crate", "visual.ref", "null"));
    EXPECT_FALSE(s->apply_patch("crate", "transform.pos", "[1, 2]"));        // arity
    EXPECT_FALSE(s->apply_patch("crate", "transform.pos", "[1, 2, 3, 4]"));
    EXPECT_FALSE(s->apply_patch("crate", "transform.pos", R"([1, "2", 3])"));
    EXPECT_FALSE(s->apply_patch("crate", "transform.scale", R"({"x":1})"));
    EXPECT_FALSE(s->apply_patch("crate", "transform.rot", "[0, 0, 0]"));     // xyz only
    EXPECT_FALSE(s->apply_patch("crate", "transform.rot", "[0, 0, 0, 0]"));  // zero length
    EXPECT_FALSE(s->apply_patch("crate", "vars/health/value", "{oops"));     // malformed
    EXPECT_EQ(s->to_json(), before);
}

TEST(SceneGraphFind, FindSceneByIdBothOverloads) {
    SceneGraph graph;
    auto a = std::make_unique<Scene>();
    a->set_id("a");
    auto b = std::make_unique<Scene>();
    b->set_id("b");
    ASSERT_TRUE(graph.mount(std::move(a)));
    ASSERT_TRUE(graph.mount(std::move(b)));

    Scene* found = graph.find_scene("b");
    ASSERT_TRUE(found != nullptr);
    EXPECT_EQ(found->id(), "b");
    const SceneGraph& cgraph = graph;
    EXPECT_TRUE(cgraph.find_scene("a") != nullptr);
    EXPECT_TRUE(cgraph.find_scene("missing") == nullptr);
    EXPECT_TRUE(graph.is_mounted("a"));
    graph.unmount("a");
    EXPECT_TRUE(graph.find_scene("a") == nullptr);
    EXPECT_FALSE(graph.is_mounted("a"));
}
