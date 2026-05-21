#include "gtest/gtest.h"

#include "ergo/render/frame_composer.h"
#include "ergo/render/render_context.h"
#include "ergo/render/render_layer.h"

#include <string>
#include <vector>

using namespace ergo::render;

namespace {

/// 呼び出し順を共有ログに記録するだけの偽レイヤー。 FrameComposer の
/// 初期化順 / 破棄順 を Vulkan 無しで検証するために使う。
class RecordingLayer : public IRenderLayer {
public:
    RecordingLayer(std::string name, std::vector<std::string>* log)
        : name_(std::move(name)), log_(log) {}

    void initialize(RenderContext&) override {
        log_->push_back("init:" + name_);
    }
    void set_render_pass(VkRenderPass) override {
        log_->push_back("setpass:" + name_);
    }
    void on_first_frame(RenderContext&) override {
        log_->push_back("firstframe:" + name_);
    }
    void update(const FrameContext&) override {
        log_->push_back("update:" + name_);
    }
    void record(VkCommandBuffer, VkExtent2D) override {
        log_->push_back("record:" + name_);
    }
    void shutdown() override {
        log_->push_back("shutdown:" + name_);
    }

private:
    std::string               name_;
    std::vector<std::string>* log_;
};

} // namespace

TEST(FrameComposer, AddPassTracksCount) {
    FrameComposer fc;
    EXPECT_EQ(fc.pass_count(), 0u);
    fc.add_pass(VK_NULL_HANDLE, {});
    fc.add_pass(VK_NULL_HANDLE, {});
    EXPECT_EQ(fc.pass_count(), 2u);
}

TEST(FrameComposer, InitializeCallsLayersInRegistrationOrder) {
    std::vector<std::string> log;
    RecordingLayer a("a", &log);
    RecordingLayer b("b", &log);
    RecordingLayer c("c", &log);

    FrameComposer fc;
    fc.add_pass(VK_NULL_HANDLE, {&a, &b});   // パス 0
    fc.add_pass(VK_NULL_HANDLE, {&c});       // パス 1

    RenderContext ctx;
    fc.initialize(ctx);

    // initialize はパス順 × レイヤー順、 その後に set_render_pass を同順で。
    ASSERT_EQ(log.size(), 6u);
    EXPECT_EQ(log[0], "init:a");
    EXPECT_EQ(log[1], "init:b");
    EXPECT_EQ(log[2], "init:c");
    EXPECT_EQ(log[3], "setpass:a");
    EXPECT_EQ(log[4], "setpass:b");
    EXPECT_EQ(log[5], "setpass:c");
}

TEST(FrameComposer, ShutdownReverseOrder) {
    std::vector<std::string> log;
    RecordingLayer a("a", &log);
    RecordingLayer b("b", &log);
    RecordingLayer c("c", &log);

    FrameComposer fc;
    fc.add_pass(VK_NULL_HANDLE, {&a, &b});
    fc.add_pass(VK_NULL_HANDLE, {&c});

    RenderContext ctx;
    fc.initialize(ctx);
    log.clear();
    fc.shutdown();

    // 破棄は登録の逆順: パス 1 から、 パス内も後ろから。
    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log[0], "shutdown:c");
    EXPECT_EQ(log[1], "shutdown:b");
    EXPECT_EQ(log[2], "shutdown:a");
}

TEST(FrameComposer, ShutdownIsIdempotent) {
    std::vector<std::string> log;
    RecordingLayer a("a", &log);

    FrameComposer fc;
    fc.add_pass(VK_NULL_HANDLE, {&a});
    RenderContext ctx;
    fc.initialize(ctx);
    log.clear();
    fc.shutdown();
    fc.shutdown();   // 2 回目は no-op
    EXPECT_EQ(log.size(), 1u);
}

TEST(FrameComposer, RunFrameWithoutVulkanContextReturnsFalse) {
    FrameComposer fc;
    RenderContext ctx;   // ctx.vk == nullptr
    fc.add_pass(VK_NULL_HANDLE, {});
    fc.initialize(ctx);
    FrameContext frame;
    // VulkanContext が無いので run_frame は安全に false を返す。
    EXPECT_FALSE(fc.run_frame(frame));
    EXPECT_EQ(fc.frame_count(), 0u);
    EXPECT_EQ(fc.last_presented_index(), UINT32_MAX);
}

TEST(FrameComposer, InitializeIsIdempotent) {
    std::vector<std::string> log;
    RecordingLayer a("a", &log);

    FrameComposer fc;
    fc.add_pass(VK_NULL_HANDLE, {&a});
    RenderContext ctx;
    fc.initialize(ctx);
    fc.initialize(ctx);   // 2 回目は no-op
    // init + setpass の 2 件のみ。
    EXPECT_EQ(log.size(), 2u);
}
