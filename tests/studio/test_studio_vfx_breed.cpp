#include "fake_ws.h"
#include "plugins/particle/particle_schema.h"
#include "plugins/vfx_breed/breed_direction.h"
#include "plugins/vfx_breed/breed_genes.h"
#include "plugins/vfx_breed/breed_genome.h"
#include "plugins/vfx_breed/breed_ops.h"
#include "plugins/vfx_breed/breed_request.h"
#include "plugins/vfx_breed/breed_rng.h"
#include "plugins/vfx_breed/vfx_breed_plugin.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::vfx_breed;
using namespace studio_test;

namespace {

class FakeRunner final : public platform::IProcessRunner {
public:
    platform::ProcessResult result;
    int                     calls = 0;
    platform::ProcessResult run(const platform::ProcessSpec&) override {
        ++calls;
        return result;
    }
};

uint32_t fixed_seed() { return 7; }

} // namespace

TEST(StudioBreedRng, MatchesTheTypeScriptSequence) {
    // Expected values from the TS mulberry32 (rng.ts) for the same seeds.
    BreedRng a(42);
    EXPECT_NEAR(a.next(), 0.60110375192016363, 1e-15);
    EXPECT_NEAR(a.next(), 0.44829055899754167, 1e-15);
    EXPECT_NEAR(a.next(), 0.85246579349040985, 1e-15);
    BreedRng zero(0);   // 0 -> golden ratio seed
    EXPECT_NEAR(zero.next(), 0.35888998024165630, 1e-15);
    BreedRng neg(-1);   // ToUint32(-1) = 0xffffffff
    EXPECT_NEAR(neg.next(), 0.89642261411063373, 1e-15);
    EXPECT_EQ(js_round(2.5), 3.0);
    EXPECT_EQ(js_round(-2.5), -2.0);
}

TEST(StudioBreedGenome, EncodeDecodeStaysInRangeAndRoundTrips) {
    EXPECT_EQ(genes().size(), 27u);
    EXPECT_EQ(gene_index("overLife.colorEnd.alpha"), 26);
    const Rgba hsv = rgba_to_hsva({1, 0.5, 0, 1});
    const Rgba back = hsva_to_rgba(hsv);
    EXPECT_NEAR(back[1], 0.5, 1e-12);
    EXPECT_EQ(fold(1.25, true), 0.25);
    EXPECT_EQ(fold(-0.25, true), 0.75);
    EXPECT_EQ(fold(7, false), 1.0);

    const JsonValue def = particle::default_effect();
    const Genome g = encode(def);
    const JsonValue cfg = decode(g, def, "x");
    EXPECT_EQ(field(*field(cfg, "emission"), "rate")->as_number(), 60.0);
    EXPECT_EQ(field(*field(cfg, "emission"), "maxAlive")->as_number(), 400.0);
    EXPECT_EQ(field(*field(cfg, "forces"), "gravity")->a->at(1).as_number(), 120.0);
    EXPECT_EQ(field(*field(cfg, "initial"), "positionShape")->as_string(), "disc");
}

TEST(StudioBreedOps, DeterministicAndNamed) {
    BreedInput in;
    in.parents    = {particle::default_effect(), parse_json(R"({"name":"fire-g3-2","emission":{"rate":200}})")};
    in.count      = 6;
    in.seed       = 1234;
    in.generation = 4;
    const auto a = breed(in);
    const auto b = breed(in);
    ASSERT_EQ(a.size(), 6u);
    for (std::size_t i = 0; i < a.size(); ++i) EXPECT_EQ(to_json(a[i].config), to_json(b[i].config));
    bool saw_fire = false;
    for (const auto& c : a) {
        EXPECT_EQ(c.parents.size(), 2u);
        EXPECT_NE(c.parents[0], c.parents[1]);
        const std::string name = field(c.config, "name")->as_string();
        if (name.rfind("fire-g4-", 0) == 0) saw_fire = true;   // old suffix replaced
        const double rate = field(*field(c.config, "emission"), "rate")->as_number();
        EXPECT_GE(rate, 1.0);
        EXPECT_LE(rate, 400.0);
    }
    EXPECT_TRUE(saw_fire);
    in.parents.clear();
    bool threw = false;
    try { breed(in); } catch (const std::invalid_argument&) { threw = true; }
    EXPECT_TRUE(threw);
    EXPECT_EQ(flip_probability(2), kMaxFlip);
}

TEST(StudioBreedDirection, WeightsBecomeBias) {
    const auto w = parse_weights("ok ```json\n{\"weights\":{\"emission.rate\":3,\"nope\":1,\"initial.size\":\"-0.5\"}}\n```");
    ASSERT_TRUE(w.has_value());
    EXPECT_EQ(w->size(), 2u);
    const auto bias = to_bias(*w);
    EXPECT_EQ(bias[gene_index("emission.rate")], 1.0);
    EXPECT_EQ(bias[gene_index("initial.size")], -0.5);
    EXPECT_EQ(bias[gene_index("initial.speedMin")], 0.0);
    EXPECT_FALSE(parse_weights("no json here").has_value());
    EXPECT_NE(build_direction_prompt("more sparkly", particle::default_effect()).find("User direction: more sparkly"),
              std::string::npos);
}

TEST(StudioBreedRequest, ValidatesAndClamps) {
    auto ok = parse_generation_request(parse_json(R"({"parents":[{"a":1},{"b":2}],"selected":[1,1],"count":"40",
                                                     "strength":-3,"generation":2.7,"direction":"  hot  "})"),
                                       fixed_seed);
    ASSERT_TRUE(std::holds_alternative<GenerationRequest>(ok));
    const auto& r = std::get<GenerationRequest>(ok);
    EXPECT_EQ(r.parents.size(), 1u);
    EXPECT_EQ(r.count, kCountMax);
    EXPECT_EQ(r.strength, 0.0);
    EXPECT_EQ(r.generation, 2);
    EXPECT_EQ(r.seed, 7.0);
    EXPECT_EQ(r.direction, "hot");
    EXPECT_TRUE(std::holds_alternative<std::string>(parse_generation_request(parse_json("[]"), fixed_seed)));
    EXPECT_TRUE(std::holds_alternative<std::string>(parse_generation_request(parse_json(R"({"parents":[1]})"), fixed_seed)));
    EXPECT_TRUE(std::holds_alternative<std::string>(
        parse_generation_request(parse_json(R"({"parents":[{}],"selected":[3]})"), fixed_seed)));
    const auto empty = std::get<GenerationRequest>(parse_generation_request(parse_json("{}"), fixed_seed));
    EXPECT_EQ(empty.parents.size(), 1u);   // the default effect
    EXPECT_EQ(empty.count, kCountDefault);
}

TEST(StudioVfxBreed, DirectionFailuresNeverStopBreeding) {
    auto runner = std::make_shared<FakeRunner>();
    runner->result.started   = true;
    runner->result.exit_code = 0;
    runner->result.out       = to_json(jobj({{"result", jstr("{\"weights\":{\"emission.rate\":1}}")}}));
    VfxBreedPlugin p({}, runner, quiet());

    JsonValue res = parse_json(p.handle(req("POST", R"({"direction":"denser","seed":5})"), "/api/generation").body);
    EXPECT_EQ(field(*field(res, "direction"), "status")->as_string(), "applied");
    EXPECT_EQ(field(res, "children")->a->size(), 8u);
    EXPECT_EQ(field(res, "generation")->as_number(), 1.0);

    runner->result.exit_code = 2;
    res = parse_json(p.handle(req("POST", R"({"direction":"denser"})"), "/api/generation").body);
    EXPECT_EQ(field(*field(res, "direction"), "status")->as_string(), "failed");
    EXPECT_EQ(field(res, "children")->a->size(), 8u);

    const int calls = runner->calls;
    res = parse_json(p.handle(req("POST", "{}"), "/api/generation").body);
    EXPECT_EQ(field(*field(res, "direction"), "status")->as_string(), "none");
    EXPECT_EQ(runner->calls, calls);   // no direction -> no LLM call

    VfxBreedSettings sdk;
    sdk.backend = "sdk";
    VfxBreedPlugin off(sdk, runner, quiet());
    res = parse_json(off.handle(req("POST", R"({"direction":"x"})"), "/api/generation").body);
    EXPECT_EQ(field(*field(res, "direction"), "status")->as_string(), "unavailable");
    EXPECT_TRUE(field(off.snapshot(), "ok")->as_bool());   // breeding itself is healthy
    EXPECT_EQ(parse_json(p.handle(req("GET"), "/api/seeds").body).a->size(), 13u);
    EXPECT_EQ(parse_json(p.handle(req("GET"), "/api/genes").body).a->size(), 27u);
}
