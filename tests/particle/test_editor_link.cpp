// ergo_particle_link — hermetic tests (no socket): frame decoding through
// decode_state_frame(), and poll() / apply_to() driven through feed().

#include "ergo/particle/editor_link.h"
#include "ergo/particle/particle_system.h"
#include "gtest/gtest.h"

#include <string>

using namespace ergo::particle;

namespace {

// Shape of the `particle` plugin's broadcast (see plugins/particle/index.ts).
std::string state_frame(const std::string& config_json) {
    return R"({"op":"state","clients":2,"config":)" + config_json + "}";
}

const char* kExplosion = R"({
    "version": 1,
    "name": "explosion",
    "emission": {"rate": 25, "maxAlive": 300},
    "initial": {"speedMin": 120, "speedMax": 300, "color": [1, 0.5, 0.25, 1]},
    "forces": {"gravity": [0, 80]},
    "render": {"blend": "alpha", "shape": "square"}
})";

} // namespace

TEST(ParticleEditorLink, StateFrameIsApplied) {
    ParticleEffectConfig c;
    ASSERT_TRUE(decode_state_frame(state_frame(kExplosion), c));
    EXPECT_EQ(c.name, "explosion");
    EXPECT_FLOAT_EQ(c.emission_rate, 25.0f);
    EXPECT_EQ(c.emission_max_alive, 300);
    EXPECT_FLOAT_EQ(c.init_speed_max, 300.0f);
    EXPECT_FLOAT_EQ(c.init_color[2], 0.25f);
    EXPECT_FLOAT_EQ(c.gravity[1], 80.0f);
    EXPECT_EQ(c.render_blend, BlendMode::Alpha);
    EXPECT_EQ(c.render_shape, ShapeMode::Square);
}

TEST(ParticleEditorLink, AckFrameIsRejected) {
    ParticleEffectConfig c;
    c.name = "keep";
    EXPECT_FALSE(decode_state_frame(R"({"op":"ack"})", c));
    // An ack carrying a config is still not a state frame.
    EXPECT_FALSE(decode_state_frame(R"({"op":"ack","config":{"name":"x"}})", c));
    EXPECT_EQ(c.name, "keep");
}

TEST(ParticleEditorLink, MalformedJsonIsRejected) {
    ParticleEffectConfig c;
    c.name = "keep";
    EXPECT_FALSE(decode_state_frame("", c));
    EXPECT_FALSE(decode_state_frame("{\"op\":\"state\",\"config\":{", c));
    EXPECT_FALSE(decode_state_frame("[1,2,3]", c));
    EXPECT_FALSE(decode_state_frame("not json", c));
    EXPECT_EQ(c.name, "keep");
}

TEST(ParticleEditorLink, FrameWithoutConfigIsRejected) {
    // Passing such a frame straight to parse_config_json would "succeed" and
    // apply nothing; decode_state_frame must say no instead.
    ParticleEffectConfig c;
    c.name = "keep";
    EXPECT_FALSE(decode_state_frame(R"({"op":"state","clients":1})", c));
    EXPECT_FALSE(decode_state_frame(R"({"op":"state","config":"oops"})", c));
    EXPECT_FALSE(decode_state_frame(R"({"op":"state","config":null})", c));
    EXPECT_EQ(c.name, "keep");
}

TEST(ParticleEditorLink, PartialConfigKeepsDefaults) {
    ParticleEffectConfig c;
    c.emission_rate = 999.0f;   // stale value from a previous effect
    ASSERT_TRUE(decode_state_frame(state_frame(R"({"emission":{"maxAlive":12}})"), c));
    const ParticleEffectConfig defaults;
    EXPECT_EQ(c.emission_max_alive, 12);
    EXPECT_FLOAT_EQ(c.emission_rate, defaults.emission_rate);
    EXPECT_EQ(c.name, defaults.name);
    EXPECT_FLOAT_EQ(c.init_size, defaults.init_size);
    EXPECT_EQ(c.render_blend, defaults.render_blend);
}

TEST(ParticleEditorLink, PollReportsOnlyNewConfigs) {
    EditorLink link;
    ParticleEffectConfig c;
    EXPECT_FALSE(link.poll(c));                         // nothing yet

    EXPECT_TRUE(link.feed(state_frame(kExplosion)));
    ASSERT_TRUE(link.poll(c));
    EXPECT_EQ(c.name, "explosion");
    EXPECT_FALSE(link.poll(c));                         // already delivered

    // Re-broadcast of the same config (a client joined): not news.
    EXPECT_FALSE(link.feed(state_frame(kExplosion)));
    EXPECT_FALSE(link.poll(c));

    EXPECT_FALSE(link.feed(R"({"op":"ack"})"));
    EXPECT_FALSE(link.poll(c));

    // Two edits between polls: the host sees the latest one.
    EXPECT_TRUE(link.feed(state_frame(R"({"name":"a"})")));
    EXPECT_TRUE(link.feed(state_frame(R"({"name":"b"})")));
    ASSERT_TRUE(link.poll(c));
    EXPECT_EQ(c.name, "b");
    EXPECT_FALSE(link.poll(c));
}

TEST(ParticleEditorLink, ApplyToSetsSystemConfig) {
    EditorLink     link;
    ParticleSystem sys;
    EXPECT_FALSE(link.apply_to(sys));
    link.feed(state_frame(R"({"name":"smoke","emission":{"rate":7}})"));
    EXPECT_TRUE(link.apply_to(sys));
    EXPECT_EQ(sys.config().name, "smoke");
    EXPECT_FLOAT_EQ(sys.config().emission_rate, 7.0f);
    EXPECT_FALSE(link.apply_to(sys));
}

TEST(ParticleEditorLink, ConnectValidatesArgumentsAndDisconnectIsIdempotent) {
    EditorLink link;
    EXPECT_FALSE(link.connect("", 5170));
    EXPECT_FALSE(link.connect("127.0.0.1", 0));
    EXPECT_FALSE(link.is_connected());
    link.disconnect();
    link.disconnect();
}
