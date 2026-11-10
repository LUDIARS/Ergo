#include "fake_ws.h"
#include "plugins/common/claude_cli.h"
#include "plugins/particle/particle_schema.h"
#include "plugins/vfx_suggest/ks_export.h"
#include "plugins/vfx_suggest/vfx_catalog.h"
#include "plugins/vfx_suggest/vfx_prompt.h"
#include "plugins/vfx_suggest/vfx_sanitize.h"
#include "plugins/vfx_suggest/vfx_suggest_plugin.h"

#include <gtest/gtest.h>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::vfx_suggest;
using namespace studio_test;

namespace {

/// Returns a canned result and records the spec it was given.
class FakeRunner final : public platform::IProcessRunner {
public:
    platform::ProcessResult result;
    platform::ProcessSpec   seen;
    int                     calls = 0;
    platform::ProcessResult run(const platform::ProcessSpec& spec) override {
        seen = spec;
        ++calls;
        return result;
    }
};

platform::ProcessResult ok_reply(const std::string& result_text) {
    platform::ProcessResult r;
    r.started   = true;
    r.exit_code = 0;
    r.out       = to_json(jobj({{"result", jstr(result_text)}}));
    return r;
}

} // namespace

TEST(StudioVfxCatalog, EmbeddedDataAndHints) {
    ASSERT_EQ(catalog().size(), 12u);
    EXPECT_EQ(catalog()[0].name, "\xE7\x82\x8E");   // 炎
    const auto ex = catalog_examples("ul", 2);
    ASSERT_EQ(ex.size(), 2u);
    EXPECT_EQ(ex[0].name, "\xE5\x9B\x9E\xE5\xBE\xA9");   // 回復 (first ul hint)
    EXPECT_TRUE(is_game_id("generic"));
    EXPECT_FALSE(is_game_id("zz"));
    const std::string sys = build_system_prompt("ac");
    EXPECT_NE(sys.find("Game: AdventureCube"), std::string::npos);
    EXPECT_NE(sys.find("Example preset:\n  name: \""), std::string::npos);
    EXPECT_EQ(build_user_content("ac", "boss hit", 2),
              "Game: ac\nScene: \"boss hit\"\n\nReturn exactly 2 distinct presets as JSON {\"presets\":[...]}.");
}

TEST(StudioVfxSanitize, ClampsAndFills) {
    const JsonValue raw = parse_json(R"([{"name":"Hit","rationale":"r","config":{
        "emission":{"rate":9999,"maxAlive":"50"},"initial":{"speedMin":-3,"color":[2,0.5,0.5,1],"lifetimeMin":"x"},
        "forces":{"gravity":[700]},"render":{"blend":"alpha","shape":"star"}}},
      {"emission":{"rate":10}}, 1, 2, 3])");
    const auto p = sanitize_presets(raw);
    ASSERT_EQ(p.size(), 4u);
    const JsonValue& c = p[0].config;
    EXPECT_EQ(field(*field(c, "emission"), "rate")->as_number(), 400.0);
    EXPECT_EQ(field(*field(c, "emission"), "maxAlive")->as_number(), 50.0);
    EXPECT_EQ(field(*field(c, "initial"), "speedMin")->as_number(), 0.0);
    EXPECT_EQ(field(*field(c, "initial"), "lifetimeMin")->as_number(), 0.7);   // default
    EXPECT_EQ(field(*field(c, "initial"), "color")->a->at(0).as_number(), 1.0);
    EXPECT_EQ(field(*field(c, "initial"), "positionShape")->as_string(), "disc");
    EXPECT_EQ(field(*field(c, "forces"), "gravity")->a->at(0).as_number(), 600.0);
    EXPECT_EQ(field(*field(c, "forces"), "gravity")->a->at(1).as_number(), 120.0);
    EXPECT_EQ(field(*field(c, "render"), "blend")->as_string(), "alpha");
    EXPECT_EQ(field(*field(c, "render"), "shape")->as_string(), "circle");
    EXPECT_EQ(p[1].name, "preset-2");   // no config: the item itself is the config
    EXPECT_EQ(field(*field(p[1].config, "emission"), "rate")->as_number(), 10.0);

    EXPECT_EQ(extract_json("text ```json\n{\"a\":1}\n``` tail"), "{\"a\":1}");
    EXPECT_EQ(extract_json("Here: {\"a\":{\"b\":2}} done"), "{\"a\":{\"b\":2}}");
}

TEST(StudioVfxKsExport, ConvertsGravityBlendAndCurves) {
    JsonValue cfg = particle::merge_config(particle::default_effect(),
                                           parse_json(R"({"name":"Bolt","forces":{"gravity":[0,98.1]},
                                                          "initial":{"lifetimeMin":1,"lifetimeMax":1}})"));
    const JsonValue e = to_ks_emitter_json(cfg, "");
    EXPECT_EQ(field(e, "name")->as_string(), "Bolt");
    EXPECT_NEAR(field(e, "gravity_modifier")->as_number(), 10.0, 1e-9);
    EXPECT_EQ(field(*field(e, "start_lifetime"), "mode")->as_number(), 0.0);   // equal -> constant
    EXPECT_EQ(field(*field(e, "size_over_lifetime"), "mode")->as_number(), 2.0);
    EXPECT_EQ(field(e, "blend_mode")->as_number(), 2.0);
    EXPECT_EQ(field(to_ks_emitter_json(cfg, "Effect/Bolt"), "name")->as_string(), "Effect/Bolt");
}

TEST(StudioClaudeCli, PassesPromptOnStdinAndRejectsBadModels) {
    auto runner = std::make_shared<FakeRunner>();
    runner->result = ok_reply("hello");
    ClaudeCli cli(runner, "claude", "haiku");
    const auto r = cli.ask("the prompt");
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.text, "hello");
    EXPECT_EQ(runner->seen.stdin_text, "the prompt");
    EXPECT_EQ(runner->seen.args.at(2), "haiku");
    ClaudeCli bad(runner, "claude", "haiku & calc");
    EXPECT_FALSE(bad.ask("x").ok);
    EXPECT_EQ(runner->calls, 1);   // never spawned
}

TEST(StudioVfxSuggest, GenerateValidatesAndSanitizes) {
    auto runner = std::make_shared<FakeRunner>();
    runner->result = ok_reply("```json\n{\"presets\":[{\"name\":\"A\",\"config\":{\"emission\":{\"rate\":5}}}]}\n```");
    VfxSuggestPlugin p({}, runner, quiet());
    EXPECT_EQ(p.handle(req("POST", R"({"game":"zz","scene":"x"})"), "/api/generate").status, 400);
    EXPECT_EQ(p.handle(req("POST", R"({"game":"ks","scene":"  "})"), "/api/generate").status, 400);
    const auto res = p.handle(req("POST", R"({"game":"ks","scene":" boss ","count":9})"), "/api/generate");
    ASSERT_EQ(res.status, 200);
    EXPECT_NE(runner->seen.stdin_text.find("Return exactly 4 distinct presets"), std::string::npos);
    EXPECT_NE(runner->seen.stdin_text.find("Scene: \"boss\""), std::string::npos);
    EXPECT_EQ(field(parse_json(res.body), "presets")->a->size(), 1u);

    runner->result.exit_code = 1;
    runner->result.err       = "boom";
    EXPECT_EQ(p.handle(req("POST", R"({"game":"ks","scene":"x"})"), "/api/generate").status, 502);

    VfxSuggestSettings sdk;
    sdk.backend = "sdk";
    VfxSuggestPlugin off(sdk, runner, quiet());
    EXPECT_FALSE(field(off.snapshot(), "ok")->as_bool());
    EXPECT_EQ(off.handle(req("POST", R"({"game":"ks","scene":"x"})"), "/api/generate").status, 503);
    EXPECT_EQ(parse_json(p.handle(req("GET"), "/api/games").body).a->size(), 4u);
}

TEST(StudioProcessQuote, WindowsRulesAndCmdSafety) {
    EXPECT_EQ(platform::quote_windows_arg("plain"), "plain");
    EXPECT_EQ(platform::quote_windows_arg("a b"), "\"a b\"");
    EXPECT_EQ(platform::quote_windows_arg("say \"hi\""), "\"say \\\"hi\\\"\"");
    EXPECT_EQ(platform::quote_windows_arg("dir\\"), "dir\\");
    EXPECT_EQ(platform::quote_windows_arg("a b\\"), "\"a b\\\\\"");
    EXPECT_EQ(platform::quote_windows_arg(""), "\"\"");
    EXPECT_TRUE(platform::cmd_safe_arg("--output-format"));
    EXPECT_FALSE(platform::cmd_safe_arg("a&b"));
    EXPECT_FALSE(platform::cmd_safe_arg("%PATH%"));
}
