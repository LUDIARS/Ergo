#include "scene_preview_raster.h"
#include <gtest/gtest.h>

using namespace ergo::studio;

namespace {
plugins::scene_editor::SceneInfo mesh_at(const char* coordinates) {
    plugins::scene_editor::SceneInfo s;
    s.camera_matrices.resize(32);
    for (int i : {0, 5, 10, 15, 16, 21, 26, 31}) s.camera_matrices[i] = 1;
    s.actors.emplace_back();
    s.actors.back().components.push_back({"EditorPreviewMesh", {{"worldTriangles", coordinates}}});
    return s;
}
}
TEST(ScenePreviewRaster, ClipsNearPlaneAndKeepsVisiblePortion) {
    const auto frame = app::raster_scene(mesh_at("[-1,-1,-1,1,-1,1,0,1,1]"),32,32);
    ASSERT_TRUE(frame.notice.empty());
    EXPECT_TRUE(frame.diffuse.at(2*32+2) < 0);
    EXPECT_TRUE(frame.diffuse.at(18*32+16) >= 0);
}
TEST(ScenePreviewRaster, RejectsIncompleteTrianglesAndUnpublishedCameras) {
    EXPECT_FALSE(app::raster_scene(mesh_at("[0,1]"),32,32).notice.empty());
    EXPECT_FALSE(app::raster_scene({},32,32).notice.empty());
}
TEST(ScenePreviewRaster, EntirelyClippedGeometryLeavesBackground) {
    const auto frame = app::raster_scene(mesh_at("[-1,-1,2,1,-1,2,0,1,2]"),32,32);
    EXPECT_TRUE(frame.notice.empty());
    EXPECT_TRUE(frame.diffuse.at(16*32+16) < 0);
}
