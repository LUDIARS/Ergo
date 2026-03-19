#include <gtest/gtest.h>
#include "ergo/compose/scene_builder.h"

#include <filesystem>
#include <fstream>

namespace ergo::compose::test {

class SceneBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "ergo_scene_test";
        std::filesystem::create_directories(testDir_);

        builder_.setActorComposer(&composer_);
        builder_.setOutputPath(testDir_.string());
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }

    void registerTestActor(const std::string& name) {
        ActorComposition actor;
        actor.actorName = name;
        composer_.registerActor(actor);
    }

    std::filesystem::path testDir_;
    ActorComposer composer_;
    SceneBuilder builder_;
};

TEST_F(SceneBuilderTest, RegisterAndGetScene) {
    SceneDefinition scene;
    scene.sceneName = "MainMenu";
    scene.targetMode = BuildMode::Web;
    scene.actorNames = {"Background", "Button"};

    builder_.registerScene(scene);

    auto* result = builder_.getScene("MainMenu");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->sceneName, "MainMenu");
    EXPECT_EQ(result->actorNames.size(), 2u);
}

TEST_F(SceneBuilderTest, UnregisterScene) {
    SceneDefinition scene;
    scene.sceneName = "TestScene";
    builder_.registerScene(scene);

    builder_.unregisterScene("TestScene");
    EXPECT_EQ(builder_.getScene("TestScene"), nullptr);
}

TEST_F(SceneBuilderTest, BuildScene_NotFound) {
    auto result = builder_.buildScene("NonExistent");
    EXPECT_FALSE(result.success);
}

TEST_F(SceneBuilderTest, BuildForWeb) {
    registerTestActor("Player");
    registerTestActor("Enemy");

    SceneDefinition scene;
    scene.sceneName = "BattleScene";
    scene.targetMode = BuildMode::Web;
    scene.actorNames = {"Player", "Enemy"};

    builder_.registerScene(scene);

    auto result = builder_.buildForWeb("BattleScene");
    EXPECT_TRUE(result.success);
}

TEST_F(SceneBuilderTest, BuildForApp) {
    registerTestActor("Player");

    SceneDefinition scene;
    scene.sceneName = "TestScene";
    scene.targetMode = BuildMode::App;
    scene.actorNames = {"Player"};

    builder_.registerScene(scene);

    auto result = builder_.buildForApp("TestScene");
    EXPECT_TRUE(result.success);
}

TEST_F(SceneBuilderTest, BuildScene_DispatchesByMode) {
    registerTestActor("Actor1");

    SceneDefinition webScene;
    webScene.sceneName = "WebScene";
    webScene.targetMode = BuildMode::Web;
    webScene.actorNames = {"Actor1"};
    builder_.registerScene(webScene);

    SceneDefinition appScene;
    appScene.sceneName = "AppScene";
    appScene.targetMode = BuildMode::App;
    appScene.actorNames = {"Actor1"};
    builder_.registerScene(appScene);

    auto webResult = builder_.buildScene("WebScene");
    auto appResult = builder_.buildScene("AppScene");

    EXPECT_TRUE(webResult.success);
    EXPECT_TRUE(appResult.success);
}

TEST_F(SceneBuilderTest, BuildScene_ActorNotRegistered) {
    SceneDefinition scene;
    scene.sceneName = "BadScene";
    scene.targetMode = BuildMode::App;
    scene.actorNames = {"NonExistentActor"};

    builder_.registerScene(scene);

    auto result = builder_.buildScene("BadScene");
    EXPECT_FALSE(result.success);
}

TEST_F(SceneBuilderTest, BuildScene_NoComposer) {
    SceneBuilder standalone;
    // ActorComposerを設定しない

    SceneDefinition scene;
    scene.sceneName = "Test";
    scene.targetMode = BuildMode::Web;
    scene.actorNames = {"Actor"};
    standalone.registerScene(scene);

    auto result = standalone.buildScene("Test");
    EXPECT_FALSE(result.success);
}

} // namespace ergo::compose::test
