#include "fake_ws.h"
#include "plugins/common/json_util.h"
#include "plugins/common/latest_blob_plugin.h"
#include "plugins/particle/particle_plugin.h"
#include "plugins/pictor_perf/pictor_perf_plugin.h"
#include "plugins/profile/profile_plugin.h"
#include "plugins/variable/variable_plugin.h"

#include <gtest/gtest.h>

#include <mutex>
#include <string>
#include <vector>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace studio_test;

namespace {
const host::LogFn kQuiet = quiet();
} // namespace

TEST(StudioVariable, EngineBindReachesUiAndUiSetReachesOwner) {
    variable::VariablePlugin p;
    Client engine = open(p, 1);
    Client ui     = open(p, 2);
    engine.h.on_text(R"({"op":"hello","role":"engine","app":"game"})");
    ui.h.on_text(R"({"op":"hello","role":"ui"})");
    ASSERT_TRUE(ui.conn->last("registry").is_object());

    const uint64_t rev0 = p.revision();
    engine.h.on_text(R"({"op":"bind","name":"hp","kind":"int32","meta":{"min":0},"value":10})");
    EXPECT_GT(p.revision(), rev0);
    const JsonValue reg = ui.conn->last("registry");
    ASSERT_EQ(field(reg, "vars")->a->size(), 1u);
    EXPECT_EQ(field(reg, "vars")->a->at(0).find("app")->as_string(), "game");
    EXPECT_TRUE(engine.conn->last("registry").is_null());   // engines get no registry frames

    engine.h.on_text(R"({"op":"value","name":"hp","value":7})");
    EXPECT_EQ(field(ui.conn->last("value"), "value")->as_number(), 7.0);

    ui.h.on_text(R"({"op":"set","name":"hp","value":3})");
    const JsonValue set = engine.conn->last("set");
    EXPECT_EQ(field(set, "name")->as_string(), "hp");
    EXPECT_EQ(field(set, "value")->as_number(), 3.0);

    EXPECT_TRUE(p.request_set("hp", jnum(4)));
    EXPECT_FALSE(p.request_set("nope", jnum(4)));
    const auto vars = p.vars();   // EXPECT_EQ binds references: keep the vector alive
    ASSERT_EQ(vars.size(), 1u);
    EXPECT_EQ(vars[0].value.as_number(), 7.0);
}

TEST(StudioVariable, EngineCloseDropsItsVarsAndActors) {
    variable::VariablePlugin p;
    Client engine = open(p, 1);
    Client ui     = open(p, 2);
    engine.h.on_text(R"({"op":"hello","role":"engine","app":"game"})");
    ui.h.on_text(R"({"op":"hello","role":"ui"})");
    engine.h.on_text(R"({"op":"bind","name":"hp","kind":"int32","meta":{},"value":1})");
    engine.h.on_text(R"({"op":"actor_register","handle":5,"parent":0,"name":"Player"})");
    ASSERT_EQ(p.actors().size(), 1u);
    EXPECT_EQ(field(ui.conn->last("actors"), "nodes")->a->at(0).find("name")->as_string(), "Player");

    engine.h.on_close();
    EXPECT_TRUE(p.vars().empty());
    EXPECT_TRUE(p.actors().empty());
    EXPECT_TRUE(field(ui.conn->last("registry"), "vars")->a->empty());
    EXPECT_EQ(field(p.snapshot(), "clients")->as_number(), 1.0);
}

TEST(StudioVariable, IgnoresUnknownRolesAndGarbage) {
    variable::VariablePlugin p;
    Client c = open(p, 1);
    c.h.on_text("not json");
    c.h.on_text(R"({"no":"op"})");
    c.h.on_text(R"({"op":"bind","name":"hp","kind":"int32","value":1})");   // before hello
    EXPECT_TRUE(p.vars().empty());
    EXPECT_EQ(p.handle(req("GET"), "/api/vars").body, "[]");
}

TEST(StudioParticle, MergeIsOneLevelDeepAndKeepsVersion) {
    const JsonValue base = particle::default_effect();
    JsonValue partial;
    ASSERT_TRUE(ergo::common::jsonm::parse(
        R"({"name":"fire","emission":{"rate":10},"forces":{"gravity":[1,2]},"unknown":1,"version":9})", partial));
    const JsonValue m = particle::merge_config(base, partial);
    EXPECT_EQ(field(m, "name")->as_string(), "fire");
    EXPECT_EQ(field(*field(m, "emission"), "rate")->as_number(), 10.0);
    EXPECT_EQ(field(*field(m, "emission"), "maxAlive")->as_number(), 400.0);
    EXPECT_EQ(field(*field(m, "forces"), "gravity")->a->at(1).as_number(), 2.0);
    EXPECT_EQ(field(m, "unknown"), nullptr);
    EXPECT_EQ(field(m, "version")->as_number(), 1.0);
    EXPECT_EQ(field(*field(base, "emission"), "rate")->as_number(), 60.0);   // base untouched
    ASSERT_NE(particle::find_range("forces.gravity.y"), nullptr);
    EXPECT_EQ(particle::find_range("forces.gravity.y")->lo, -600.0);
    EXPECT_EQ(particle::find_range("nope"), nullptr);
}

TEST(StudioParticle, ClientsReceiveStateOnEveryChange) {
    particle::ParticlePlugin p;
    Client a = open(p, 1);
    EXPECT_EQ(field(a.conn->last("state"), "clients")->as_number(), 1.0);
    Client b = open(p, 2);
    EXPECT_EQ(field(a.conn->last("state"), "clients")->as_number(), 2.0);

    b.h.on_text(R"({"op":"set","config":{"name":"smoke"}})");
    EXPECT_EQ(field(*field(a.conn->last("state"), "config"), "name")->as_string(), "smoke");
    b.h.on_text(R"({"op":"replace","config":{"emission":{"rate":5}}})");
    const JsonValue cfg = *field(a.conn->last("state"), "config");
    EXPECT_EQ(field(cfg, "name")->as_string(), "untitled");
    EXPECT_EQ(field(*field(cfg, "emission"), "rate")->as_number(), 5.0);
    b.h.on_text(R"({"op":"ping"})");
    EXPECT_TRUE(b.conn->last("ack").is_object());

    EXPECT_EQ(p.handle(req("POST", "[1]"), "/api/effect").status, 400);
    EXPECT_EQ(p.handle(req("POST", R"({"name":"x"})"), "/api/effect").status, 200);
    EXPECT_EQ(field(p.config(), "name")->as_string(), "x");
    b.h.on_close();
    EXPECT_EQ(field(a.conn->last("state"), "clients")->as_number(), 1.0);
}

TEST(StudioPictorPerf, ValidatesAndServesSnapshots) {
    EXPECT_TRUE(pictor_perf::is_perf_snapshot(R"({"memory":{}})"));
    EXPECT_FALSE(pictor_perf::is_perf_snapshot(R"({"x":1})"));
    EXPECT_FALSE(pictor_perf::is_perf_snapshot("[]"));

    auto plugin = pictor_perf::make_pictor_perf_plugin(kQuiet);
    auto& http  = *plugin->http();
    EXPECT_EQ(http.handle(req("GET"), "/api/snapshot").status, 404);
    EXPECT_EQ(http.handle(req("POST", R"({"x":1})"), "/api/publish").status, 400);

    Client ui = open(*plugin->socket(), 1);
    EXPECT_TRUE(field(ui.conn->last("snapshot"), "snapshot")->is_null());
    EXPECT_EQ(http.handle(req("POST", R"({"memory":{"a":1}})"), "/api/publish").status, 200);
    EXPECT_EQ(field(ui.conn->last("snapshot"), "snapshot")->as_string(), R"({"memory":{"a":1}})");
    EXPECT_EQ(http.handle(req("GET"), "/api/snapshot").body, R"({"memory":{"a":1}})");
    EXPECT_TRUE(field(plugin->health()->snapshot(), "has_snapshot")->as_bool());

    ui.h.on_text(R"({"op":"publish","snapshot":"{\"bad\":1}"})");   // rejected, kept old one
    EXPECT_EQ(http.handle(req("GET"), "/api/snapshot").body, R"({"memory":{"a":1}})");
}

TEST(StudioProfile, PublishedTraceGoesToEveryClient) {
    auto plugin = profile::make_profile_plugin(kQuiet);
    Client a = open(*plugin->socket(), 1);
    Client b = open(*plugin->socket(), 2);
    a.h.on_text(R"({"op":"publish","trace":"{\"traceEvents\":[]}"})");
    EXPECT_EQ(field(b.conn->last("trace"), "trace")->as_string(), R"({"traceEvents":[]})");
    EXPECT_TRUE(field(plugin->health()->snapshot(), "has_trace")->as_bool());
    // profile has no HTTP publish route.
    EXPECT_EQ(plugin->http()->handle(req("POST", "{}"), "/api/publish").status, 404);
}
