#include "fake_ws.h"
#include "plugins/scene_editor/parse_scene.h"
#include "plugins/scene_editor/scene_editor_plugin.h"

#include <gtest/gtest.h>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::scene_editor;
using namespace studio_test;

TEST(StudioSceneParse, FillsDefaultsAndKeepsFreeFormAsJsonText) {
    const auto s = parse_scene("relay", R"({
        "id":"doc","mount":"/lvl",
        "actors":[
          {"id":"a","name":5,"parent":"","transform":{"pos":[1,2,3],"rot":[0,"x",0,1]},
           "vars":[{"name":"hp","type":"int","value":{"max":3}}],
           "components":[{"type":"Light","color":[1,0,0]}],
           "overrides":[{"path":"name","value":"X"}]},
          7
        ],
        "camera":{"distance":3}})",
                               "game");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->id, "relay");       // the relay key wins
    EXPECT_EQ(s->domain, "level");
    EXPECT_EQ(s->mount, "/lvl");
    EXPECT_EQ(s->app, "game");
    ASSERT_EQ(s->actors.size(), 1u); // non-object actors are skipped
    const ActorSummary& a = s->actors[0];
    EXPECT_EQ(a.name, "5");          // non-string scalar as JSON text
    EXPECT_EQ(a.type, "Actor");
    EXPECT_EQ(a.pos[2], 3.0);
    EXPECT_EQ(a.rot[3], 1.0);        // a vector with a non-number falls back
    EXPECT_EQ(a.scale[0], 1.0);
    EXPECT_EQ(a.vars[0].value, "{\"max\":3}");
    ASSERT_EQ(a.components[0].props.size(), 1u);
    EXPECT_EQ(a.components[0].props[0].first, "color");
    EXPECT_EQ(a.overrides[0].value, "\"X\"");
    EXPECT_EQ(s->camera_mode, "orbit");
    EXPECT_EQ(s->camera_distance, 3.0);
    EXPECT_EQ(s->camera_fov_deg, 50.0);

    EXPECT_FALSE(parse_scene("x", "[1]", "g").has_value());
    EXPECT_FALSE(parse_scene("x", "{oops", "g").has_value());
}

TEST(StudioSceneParse, RelaysCameraPoseAndClipPlanesToBothEditors) {
    const auto scene = parse_scene("station", R"({"camera":{"mode":"fly","position":[2,3,4],
        "front":[1,0,0],"yaw":0.75,"pitch":-0.25,"z_near":0.5,"z_far":150}})", "game");
    ASSERT_TRUE(scene);
    const auto wire = to_json_value(*scene);
    const auto* camera = wire.find("camera");
    ASSERT_NE(camera, nullptr);
    for (const char* field : {"position", "front", "yaw", "pitch", "z_near", "z_far"}) {
        ASSERT_NE(camera->find(field), nullptr);
    }
    EXPECT_DOUBLE_EQ(camera->find("position")->a->at(2).n, 4);
    EXPECT_DOUBLE_EQ(camera->find("front")->a->at(0).n, 1);
    EXPECT_DOUBLE_EQ(camera->find("yaw")->n, 0.75);
    EXPECT_DOUBLE_EQ(camera->find("pitch")->n, -0.25);
    EXPECT_DOUBLE_EQ(camera->find("z_near")->n, 0.5);
    EXPECT_DOUBLE_EQ(camera->find("z_far")->n, 150);
}

TEST(StudioSceneEditor, RelaysScenesAndPatches) {
    SceneEditorPlugin p(quiet());
    Client engine = open(p, 1);
    Client ui     = open(p, 2);
    engine.h.on_text(R"({"op":"hello","role":"engine","app":"game"})");
    ui.h.on_text(R"({"op":"hello","role":"ui"})");
    EXPECT_TRUE(field(ui.conn->last("scenes"), "scenes")->a->empty());

    engine.h.on_text(R"({"op":"scene","id":"main","json":"{\"actors\":[{\"id\":\"p\"}]}"})");
    const JsonValue upd = ui.conn->last("scene_update");
    EXPECT_EQ(field(*field(upd, "scene"), "app")->as_string(), "game");
    ASSERT_EQ(p.scenes().size(), 1u);

    ui.h.on_text(R"({"op":"patch","scene_id":"main","actor_id":"p","field":"name","value":"Hero"})");
    const JsonValue patch = engine.conn->last("patch");
    EXPECT_EQ(field(patch, "actor_id")->as_string(), "p");
    EXPECT_EQ(field(patch, "value")->as_string(), "Hero");
    EXPECT_FALSE(p.patch("nope", "p", "name", jstr("x")));

    engine.h.on_text(R"({"op":"scene","id":"bad","json":"not json"})");
    EXPECT_EQ(p.scenes().size(), 1u);

    engine.h.on_close();
    EXPECT_TRUE(p.scenes().empty());
    EXPECT_EQ(field(ui.conn->last("scene_removed"), "id")->as_string(), "main");
}

TEST(StudioSceneEditor, IgnoresScenesBeforeHello) {
    SceneEditorPlugin p(quiet());
    Client c = open(p, 1);
    c.h.on_text(R"({"op":"scene","id":"main","json":"{}"})");
    EXPECT_TRUE(p.scenes().empty());
    EXPECT_EQ(p.handle(req("GET"), "/api/scenes").body, "[]");
}

TEST(StudioSceneEditor, PfReadsAreScopedToTheOpenProfile) {
    ProfileSet set;
    set.profiles.emplace_back();
    set.profiles.front().praeforma_project = "game-a";
    auto plugin = make_scene_editor_plugin({}, std::make_shared<FixedProfiles>(std::move(set)), quiet());
    net::HttpRequest req;
    req.method = "GET";
    EXPECT_EQ(plugin->http()->handle(req, "/api/praeforma/game-b/specs/spec1").status, 404);
    EXPECT_EQ(plugin->http()->handle(req, "/api/praeforma/game-a/specs/../game-b").status, 400);
    req.method = "POST";
    EXPECT_EQ(plugin->http()->handle(req, "/api/praeforma").status, 405);
}
