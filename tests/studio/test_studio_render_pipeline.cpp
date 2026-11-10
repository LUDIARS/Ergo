#include "fake_ws.h"
#include "plugins/common/json_pretty.h"
#include "plugins/render_pipeline/profile_schema.h"
#include "plugins/render_pipeline/profile_store.h"
#include "plugins/render_pipeline/render_pipeline_plugin.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::render_pipeline;
using namespace studio_test;
namespace fs = std::filesystem;

TEST(StudioJsonPretty, MatchesJsonStringifyLayout) {
    EXPECT_EQ(number_text(1.0), "1");
    EXPECT_EQ(number_text(0.025), "0.025");
    EXPECT_EQ(number_text(268435456), "268435456");
    EXPECT_EQ(number_text(-0.0), "0");
    OrderedJson o = OrderedJson::object();
    o.add("b", OrderedJson::from(jnum(1))).add("a", OrderedJson::from(JsonValue::make_array()));
    OrderedJson arr = OrderedJson::array();
    arr.push(OrderedJson::from(jstr("x")));
    o.add("c", std::move(arr));
    EXPECT_EQ(o.pretty(), "{\n  \"b\": 1,\n  \"a\": [],\n  \"c\": [\n    \"x\"\n  ]\n}");
}

TEST(StudioProfileSchema, FillsDefaultsAndUpgradesV1) {
    JsonValue raw;
    ASSERT_TRUE(ergo::common::jsonm::parse(
        R"({"version":1,"profile_name":"Mobile","rendering_path":"WARP","unknown":1,
            "render_passes":[{"pass_name":"Opaque","pass_type":"BOGUS","render_targets":["a",3]}],
            "post_process":[{"name":"tonemap"},{"name":"SSAO"},{"name":"x","kind":"Vignette"}],
            "gi":{"ssao":{"bias":"no"}},"_editor":{"nodePositions":{"Opaque":{"x":"5","y":2},"bad":{"x":1}}}})",
        raw));
    const JsonValue p = normalize_profile(raw);
    EXPECT_EQ(field(p, "version")->as_number(), 2.0);
    EXPECT_EQ(field(p, "rendering_path")->as_string(), "FORWARD_PLUS");   // unknown enum -> default
    EXPECT_EQ(field(p, "unknown"), nullptr);
    EXPECT_EQ(field(p, "attachments")->a->size(), 3u);                  // v1 -> built-in three
    const JsonValue& pass = field(p, "render_passes")->a->at(0);
    EXPECT_EQ(field(pass, "pass_type")->as_string(), "OPAQUE");
    EXPECT_EQ(field(pass, "render_targets")->a->size(), 1u);
    EXPECT_EQ(field(pass, "filter_mask")->as_number(), 65535.0);
    const auto& pp = *field(p, "post_process")->a;
    EXPECT_EQ(field(pp[0], "kind")->as_string(), "ToneMapping");
    EXPECT_EQ(field(pp[1], "kind")->as_string(), "Unknown");
    EXPECT_EQ(field(pp[2], "kind")->as_string(), "Vignette");           // explicit kind wins
    EXPECT_EQ(field(*field(*field(p, "gi"), "ssao"), "bias")->as_number(), 0.025);
    const JsonValue* pos = field(*field(p, "_editor"), "nodePositions");
    ASSERT_NE(pos, nullptr);
    EXPECT_EQ(field(*field(*pos, "Opaque"), "x")->as_number(), 5.0);
    EXPECT_EQ(field(*pos, "bad"), nullptr);
}

TEST(StudioProfileSchema, SerializesInCanonicalOrderAndRoundTrips) {
    JsonValue raw;
    ASSERT_TRUE(ergo::common::jsonm::parse(
        R"({"profile_name":"Std","post_process":[{"name":"Bloom"},{"name":"SSAO"}],
            "render_passes":[{"pass_name":"P","attachment_ops":[{"attachment":"swapchain","final_layout":"PRESENT_SRC_KHR"}]},
                             {"pass_name":"Q"}]})",
        raw));
    const std::string text = serialize_profile(normalize_profile(raw));
    EXPECT_EQ(text.rfind("{\n  \"version\": 2,\n  \"profile_name\": \"Std\",\n  \"rendering_path\": \"FORWARD_PLUS\",", 0), 0u);
    EXPECT_EQ(text.back(), '\n');
    EXPECT_NE(text.find("\"bias\": 0.025"), std::string::npos);
    EXPECT_NE(text.find("\"input_textures\": []"), std::string::npos);
    // Only the block of the effect's kind is written; Unknown carries none.
    EXPECT_EQ(text.find("\"tone_mapping\""), std::string::npos);
    EXPECT_NE(text.find("\"bloom\": {"), std::string::npos);
    // attachment_ops only on the pass that has them.
    EXPECT_EQ(text.find("\"attachment_ops\""), text.rfind("\"attachment_ops\""));
    // The memory block keeps gpu last, like the presets.
    EXPECT_LT(text.find("\"use_large_pages\""), text.find("\"gpu\": {"));
    EXPECT_EQ(text.find("_editor"), std::string::npos);

    JsonValue again;
    ASSERT_TRUE(ergo::common::jsonm::parse(text, again));
    EXPECT_EQ(serialize_profile(normalize_profile(again)), text);
    EXPECT_EQ(profile_file_name("My Profile!"), "myprofile.profile.json");
    EXPECT_EQ(profile_file_name("  !! "), "untitled.profile.json");
}

TEST(StudioProfileStore, SaveListLoadAndRejectUnsafeNames) {
    const fs::path dir = fs::temp_directory_path() / "ergo_studio_profiles";
    fs::remove_all(dir);
    RenderPipelinePlugin p(dir, quiet());
    Client c = open(p, 1);
    const auto r = p.save(jobj({{"profile_name", jstr("Forward")}}), "");
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.file, "forward.profile.json");
    EXPECT_EQ(field(c.conn->last("profiles-changed"), "file")->as_string(), "forward.profile.json");
    const auto list = p.store().list();
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].profile_name, "Forward");
    EXPECT_EQ(p.handle(req("GET"), "/api/profile/forward.profile.json").status, 200);
    EXPECT_EQ(p.handle(req("GET"), "/api/profile/missing.profile.json").status, 404);
    EXPECT_EQ(p.handle(req("GET"), "/api/profile/..%2Fx.profile.json").status, 400);
    EXPECT_FALSE(is_safe_profile_file("../x.profile.json"));
    EXPECT_FALSE(is_safe_profile_file("C:x.profile.json"));
    EXPECT_FALSE(is_safe_profile_file(".profile.json"));
    EXPECT_EQ(p.handle(req("POST", R"({"file":"a.json"})"), "/api/profile").status, 400);

    c.h.on_text(R"({"op":"timing","frame":3,"passes":[]})");
    EXPECT_EQ(field(p.last_timing(), "frame")->as_number(), 3.0);
    EXPECT_EQ(field(c.conn->last("timing"), "frame")->as_number(), 3.0);
    fs::remove_all(dir);

    auto unset = make_render_pipeline_plugin("", quiet());
    EXPECT_EQ(unset->http()->handle(req("GET"), "/api/profiles").status, 503);
    EXPECT_NE(field(unset->health()->snapshot(), "profile_dir_err"), nullptr);
}
