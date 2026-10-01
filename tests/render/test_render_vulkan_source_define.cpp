#include "gtest/gtest.h"

#include <string>

// ergo_render_vulkan_source_definition() が作る定義の回帰テスト。
// CMake 側で出所名 "Vulkan SDK" (空白入り) をこの翻訳単位へ付けてビルドする。
// 定義値を二重にエスケープすると MSVC でここがコンパイルエラーになる。
// 実描画 (pictor) の有無に関係なくビルドされるので、 Ergo 単体の CI でも
// 定義の作り方を検査できる。

#ifndef ERGO_RENDER_VULKAN_SOURCE
#error "ERGO_RENDER_VULKAN_SOURCE must be defined by ergo_render_vulkan_source_definition()"
#endif

TEST(RenderVulkanSourceDefine, ExpandsToStringLiteralWithSpaces) {
    const std::string source = ERGO_RENDER_VULKAN_SOURCE;
    EXPECT_EQ(source, "Vulkan SDK");
}
