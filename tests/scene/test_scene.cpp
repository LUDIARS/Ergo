#include "gtest/gtest.h"

#include "ergo/scene/scene.h"

#include <string>

using namespace ergo::scene;

namespace {

constexpr float kEps = 1e-4f;

const char* sample_scene_json() {
    return R"JSON({
  "version": 1,
  "id": "stage_01",
  "domain": "level",
  "mount": "/level",
  "imports": ["prefabs/enemy.scene.json"],
  "camera": {
    "mode": "orbit",
    "target": [1, 2, 3],
    "distance": 8.0,
    "yaw": 0.6,
    "pitch": 0.3,
    "fov_deg": 50
  },
  "post_process": {
    "tonemap": "aces",
    "exposure": 1.25,
    "bloom": { "enabled": true, "threshold": 1.0, "intensity": 0.6 }
  },
  "actors": [
    {
      "id": "ground",
      "type": "StaticMeshActor",
      "parent": null,
      "transform": { "pos": [0,0,0], "rot": [0,0,0,1], "scale": [10,1,10] },
      "visual": { "kind": "mesh", "ref": "assets/ground.mesh", "material": "assets/ground.mat" }
    },
    {
      "id": "crate",
      "type": "PropActor",
      "parent": "ground",
      "transform": { "pos": [1,2,3], "rot": [0,0,0,1], "scale": [1,1,1] },
      "visual": { "kind": "mesh", "ref": "assets/crate.mesh", "material": "assets/crate.mat" }
    }
  ]
})JSON";
}

} // namespace

TEST(SceneDocument, EmptySceneDefaults) {
    Scene scene;
    EXPECT_EQ(scene.actor_count(), 0u);
    EXPECT_EQ(scene.domain(), "level");
    EXPECT_EQ(scene.mount(), "/level");
    EXPECT_EQ(scene.camera().mode, CameraMode::Orbit);
    EXPECT_EQ(scene.camera().distance, 8.0f);
    EXPECT_EQ(scene.post_process().tonemap, "none");
}

TEST(SceneDocument, LoadJsonActorsAreAddressable) {
    auto scene = Scene::load_json(sample_scene_json());
    ASSERT_TRUE(scene != nullptr);
    EXPECT_EQ(scene->id(), "stage_01");
    EXPECT_EQ(scene->domain(), "level");
    EXPECT_EQ(scene->mount(), "/level");
    ASSERT_EQ(scene->imports().size(), 1u);
    EXPECT_EQ(scene->imports()[0], "prefabs/enemy.scene.json");
    EXPECT_EQ(scene->actor_count(), 2u);
    EXPECT_EQ(scene->camera().mode, CameraMode::Orbit);
    EXPECT_FLOAT_EQ(scene->camera().target.x, 1.0f);
    EXPECT_FLOAT_EQ(scene->post_process().exposure, 1.25f);
    EXPECT_TRUE(scene->post_process().bloom.enabled);

    const ActorSpec* ground = scene->find_actor("ground");
    ASSERT_TRUE(ground != nullptr);
    EXPECT_EQ(ground->type, "StaticMeshActor");
    EXPECT_EQ(ground->visual.kind, "mesh");
    EXPECT_EQ(ground->visual.ref, "assets/ground.mesh");
    EXPECT_FLOAT_EQ(ground->transform.scale.x, 10.0f);

    const ActorSpec* crate = scene->find_actor("crate");
    ASSERT_TRUE(crate != nullptr);
    EXPECT_EQ(crate->type, "PropActor");
    EXPECT_EQ(crate->parent, "ground");
    EXPECT_FLOAT_EQ(crate->transform.position.z, 3.0f);
}

TEST(SceneDocument, RoundTripPreservesCoreFields) {
    auto scene = Scene::load_json(sample_scene_json());
    ASSERT_TRUE(scene != nullptr);
    const std::string json = scene->to_json();
    auto copy = Scene::load_json(json);
    ASSERT_TRUE(copy != nullptr);

    EXPECT_EQ(copy->actor_count(), 2u);
    ASSERT_TRUE(copy->find_actor("ground") != nullptr);
    ASSERT_TRUE(copy->find_actor("crate") != nullptr);
    EXPECT_EQ(copy->find_actor("crate")->parent, "ground");
    EXPECT_TRUE(json.find("\"actors\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"objects\"") == std::string::npos);
    EXPECT_EQ(copy->post_process().tonemap, "aces");
    EXPECT_FLOAT_EQ(copy->post_process().bloom.intensity, 0.6f);
}

TEST(SceneDocument, CrudRejectsDuplicateIdsAndDetachesChildrenOnRemove) {
    Scene scene;
    ActorSpec parent;
    parent.id = "parent";
    ActorSpec child;
    child.id = "child";
    child.parent = "parent";

    EXPECT_TRUE(scene.add_actor(parent));
    EXPECT_TRUE(scene.add_actor(child));
    EXPECT_FALSE(scene.add_actor(parent));
    EXPECT_EQ(scene.actor_count(), 2u);

    EXPECT_TRUE(scene.remove_actor("parent"));
    EXPECT_EQ(scene.actor_count(), 1u);
    ASSERT_TRUE(scene.find_actor("child") != nullptr);
    EXPECT_EQ(scene.find_actor("child")->parent, "");
    EXPECT_FALSE(scene.remove_actor("missing"));
}

TEST(SceneDocument, LegacyObjectsJsonLoadsAsActors) {
    auto scene = Scene::load_json(R"JSON({
      "version": 1,
      "objects": [
        { "id": "legacy", "transform": { "pos": [4,5,6] } }
      ]
    })JSON");
    ASSERT_TRUE(scene != nullptr);
    EXPECT_EQ(scene->actor_count(), 1u);
    ASSERT_TRUE(scene->find_actor("legacy") != nullptr);
    EXPECT_EQ(scene->find_actor("legacy")->type, "Actor");
    EXPECT_FLOAT_EQ(scene->find_actor("legacy")->transform.position.y, 5.0f);
}

TEST(ActorSpecComponents, RoundTripComponentsVarsInstanceOf) {
    const char* json = R"JSON({
      "version": 1,
      "actors": [
        {
          "id": "enemy",
          "type": "EnemyActor",
          "components": [
            { "type": "Renderable", "mesh": "assets/enemy.mesh", "material": "assets/enemy.mat" },
            { "type": "Collider", "shape": "capsule", "radius": 0.5 },
            { "type": "Behavior", "script": "scripts/enemy_ai.lua" }
          ],
          "vars": [
            { "name": "health", "type": "int", "value": 100 },
            { "name": "tag", "type": "string", "value": "enemy" }
          ],
          "instanceOf": "prefabs/base_enemy",
          "overrides": [
            { "path": "vars/health", "value": 50 }
          ]
        }
      ]
    })JSON";

    auto scene = Scene::load_json(json);
    ASSERT_TRUE(scene != nullptr);
    const ActorSpec* e = scene->find_actor("enemy");
    ASSERT_TRUE(e != nullptr);

    // components
    ASSERT_EQ(e->components.size(), 3u);
    EXPECT_EQ(e->components[0].type, "Renderable");
    EXPECT_EQ(e->components[1].type, "Collider");
    EXPECT_EQ(e->components[2].type, "Behavior");
    // props on Renderable (map order: material < mesh)
    ASSERT_EQ(e->components[0].props.size(), 2u);
    const auto has_prop = [](const ComponentSpec& cs, const std::string& key) {
        for (const auto& [k, v] : cs.props) { if (k == key) return true; }
        return false;
    };
    EXPECT_TRUE(has_prop(e->components[0], "mesh"));
    EXPECT_TRUE(has_prop(e->components[0], "material"));

    // vars
    ASSERT_EQ(e->vars.size(), 2u);
    EXPECT_EQ(e->vars[0].name, "health");
    EXPECT_EQ(e->vars[0].type, "int");
    EXPECT_EQ(e->vars[0].value, "100");
    EXPECT_EQ(e->vars[1].name, "tag");
    EXPECT_EQ(e->vars[1].value, "\"enemy\"");

    // instanceOf + overrides
    EXPECT_EQ(e->instance_of, "prefabs/base_enemy");
    ASSERT_EQ(e->overrides.size(), 1u);
    EXPECT_EQ(e->overrides[0].path, "vars/health");
    EXPECT_EQ(e->overrides[0].value, "50");

    // round-trip
    auto copy = Scene::load_json(scene->to_json());
    ASSERT_TRUE(copy != nullptr);
    const ActorSpec* ec = copy->find_actor("enemy");
    ASSERT_TRUE(ec != nullptr);
    EXPECT_EQ(ec->components.size(), 3u);
    EXPECT_EQ(ec->vars.size(), 2u);
    EXPECT_EQ(ec->instance_of, "prefabs/base_enemy");
    EXPECT_EQ(ec->overrides.size(), 1u);
    EXPECT_EQ(ec->vars[0].value, "100");
    EXPECT_EQ(ec->overrides[0].value, "50");
}

TEST(ActorSpecComponents, EmptyComponentsAndVarsNotSerialised) {
    ActorSpec actor;
    actor.id = "plain";
    Scene scene;
    scene.add_actor(actor);
    const std::string json = scene.to_json();
    EXPECT_EQ(json.find("components"), std::string::npos);
    EXPECT_EQ(json.find("vars"), std::string::npos);
    EXPECT_EQ(json.find("instanceOf"), std::string::npos);
    EXPECT_EQ(json.find("overrides"), std::string::npos);
}

TEST(SceneGraphTest, MountUnmountAndTraversal) {
    SceneGraph graph;
    EXPECT_EQ(graph.scene_count(), 0u);

    auto s1 = Scene::load_json(R"JSON({"version":1,"id":"scene_a","actors":[{"id":"actor1"},{"id":"actor2"}]})JSON");
    auto s2 = Scene::load_json(R"JSON({"version":1,"id":"scene_b","actors":[{"id":"actor3"}]})JSON");
    ASSERT_TRUE(s1 != nullptr);
    ASSERT_TRUE(s2 != nullptr);

    EXPECT_TRUE(graph.mount(std::move(s1)));
    EXPECT_EQ(graph.scene_count(), 1u);
    EXPECT_TRUE(graph.is_mounted("scene_a"));
    EXPECT_FALSE(graph.is_mounted("scene_b"));

    EXPECT_TRUE(graph.mount(std::move(s2)));
    EXPECT_EQ(graph.scene_count(), 2u);

    // duplicate mount returns false
    auto dup = Scene::load_json(R"JSON({"version":1,"id":"scene_a","actors":[]})JSON");
    EXPECT_FALSE(graph.mount(std::move(dup)));
    EXPECT_EQ(graph.scene_count(), 2u);

    // find_actor across scenes
    EXPECT_NE(graph.find_actor("actor1"), nullptr);
    EXPECT_NE(graph.find_actor("actor3"), nullptr);
    EXPECT_EQ(graph.find_actor("missing"), nullptr);

    // for_each_actor visits all 3
    int count = 0;
    graph.for_each_actor([&](const Scene&, const ActorSpec&) { ++count; });
    EXPECT_EQ(count, 3);

    EXPECT_TRUE(graph.unmount("scene_a"));
    EXPECT_EQ(graph.scene_count(), 1u);
    EXPECT_FALSE(graph.is_mounted("scene_a"));
    EXPECT_EQ(graph.find_actor("actor1"), nullptr);

    EXPECT_FALSE(graph.unmount("nonexistent"));
}

TEST(SceneCamera, OrbitAndProjectionMatricesAreUsable) {
    Camera camera;
    camera.mode = CameraMode::Orbit;
    camera.target = Vec3{0.0f, 0.0f, 0.0f};
    camera.distance = 5.0f;
    camera.yaw = 0.0f;
    camera.pitch = 0.0f;

    float view[16];
    camera.view_matrix(view);
    EXPECT_NEAR(view[14], -5.0f, kEps);

    float proj[16];
    camera.projection_matrix(16.0f / 9.0f, proj);
    EXPECT_GT(proj[0], 0.0f);
    EXPECT_LT(proj[5], 0.0f);
    EXPECT_FLOAT_EQ(proj[11], -1.0f);
    EXPECT_LT(proj[10], 0.0f);
}
