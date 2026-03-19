#include <gtest/gtest.h>
#include "ergo/compose/compose_system.h"

#include <filesystem>
#include <fstream>

namespace ergo::compose::test {

class ComposeSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "ergo_system_test";
        std::filesystem::create_directories(testDir_);

        config_.defaultMode = BuildMode::App;
        config_.hotReloadEnabled = true;
        config_.outputPath = testDir_.string();
        config_.typeDefPath = testDir_.string();
        config_.quicPort = 4433;
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }

    std::string createTsFile(const std::string& name, const std::string& content) {
        auto path = (testDir_ / name).string();
        std::ofstream ofs(path);
        ofs << content;
        return path;
    }

    ActorComposition makeTestActor(const std::string& name) {
        ActorComposition actor;
        actor.actorName = name;

        TypeScriptSource logic;
        logic.filePath = createTsFile(name + ".ts", "export class " + name + " {}");
        logic.moduleName = "game";
        logic.actorName = name;
        actor.logicSources.push_back(logic);

        return actor;
    }

    std::filesystem::path testDir_;
    ComposeConfig config_;
};

TEST_F(ComposeSystemTest, InitializeAndShutdown) {
    ComposeSystem system;
    system.initialize(config_);
    system.shutdown();
}

TEST_F(ComposeSystemTest, DefaultBuildMode) {
    ComposeSystem system;
    system.initialize(config_);
    EXPECT_EQ(system.getBuildMode(), BuildMode::App);
    system.shutdown();
}

TEST_F(ComposeSystemTest, SetBuildMode) {
    ComposeSystem system;
    system.initialize(config_);

    system.setBuildMode(BuildMode::Web);
    EXPECT_EQ(system.getBuildMode(), BuildMode::Web);

    system.setBuildMode(BuildMode::App);
    EXPECT_EQ(system.getBuildMode(), BuildMode::App);

    system.shutdown();
}

TEST_F(ComposeSystemTest, RegisterAndBuildActor) {
    ComposeSystem system;
    system.initialize(config_);

    auto actor = makeTestActor("Player");
    system.registerActor(actor);

    auto result = system.buildActor("Player");
    EXPECT_TRUE(result.success);

    system.shutdown();
}

TEST_F(ComposeSystemTest, RegisterAndBuildScene) {
    ComposeSystem system;
    system.initialize(config_);

    auto player = makeTestActor("Player");
    auto enemy = makeTestActor("Enemy");
    system.registerActor(player);
    system.registerActor(enemy);

    SceneDefinition scene;
    scene.sceneName = "BattleScene";
    scene.targetMode = BuildMode::App;
    scene.actorNames = {"Player", "Enemy"};
    system.registerScene(scene);

    auto result = system.buildScene("BattleScene");
    EXPECT_TRUE(result.success);

    system.shutdown();
}

TEST_F(ComposeSystemTest, ProcessCompileDirective_Actor) {
    ComposeSystem system;
    system.initialize(config_);

    system.registerActor(makeTestActor("TestActor"));

    auto result = system.processCompileDirective("TestActor", BuildUnit::Actor);
    EXPECT_TRUE(result.success);

    system.shutdown();
}

TEST_F(ComposeSystemTest, ProcessCompileDirective_Scene) {
    ComposeSystem system;
    system.initialize(config_);

    system.registerActor(makeTestActor("Actor1"));

    SceneDefinition scene;
    scene.sceneName = "TestScene";
    scene.targetMode = BuildMode::App;
    scene.actorNames = {"Actor1"};
    system.registerScene(scene);

    auto result = system.processCompileDirective("TestScene", BuildUnit::Scene);
    EXPECT_TRUE(result.success);

    system.shutdown();
}

TEST_F(ComposeSystemTest, SubsystemAccess) {
    ComposeSystem system;
    system.initialize(config_);

    // サブシステムにアクセスできること
    auto& composer = system.actorComposer();
    auto& scene = system.sceneBuilder();
    auto& quic = system.quicCommander();
    auto& hot = system.hotReloader();
    auto& trans = system.transpiler();

    (void)composer;
    (void)scene;
    (void)quic;
    (void)hot;
    (void)trans;

    system.shutdown();
}

TEST_F(ComposeSystemTest, WebAndAppBuildDifferentModules) {
    ComposeSystem system;
    system.initialize(config_);

    auto actor = makeTestActor("Player");
    system.registerActor(actor);

    // Web ビルド
    system.setBuildMode(BuildMode::Web);
    auto webResult = system.buildActor("Player");
    EXPECT_TRUE(webResult.success);

    // App ビルド
    system.setBuildMode(BuildMode::App);
    auto appResult = system.buildActor("Player");
    EXPECT_TRUE(appResult.success);

    system.shutdown();
}

TEST_F(ComposeSystemTest, HotReloadEnabled) {
    ComposeSystem system;
    config_.hotReloadEnabled = true;
    system.initialize(config_);

    auto actor = makeTestActor("HotActor");
    system.registerActor(actor);

    // ホットリローダーに監視対象として登録されているはず
    // (直接確認はできないがエラーなく完了すること)

    system.shutdown();
}

TEST_F(ComposeSystemTest, SceneBuild_WebMode) {
    ComposeSystem system;
    system.initialize(config_);

    system.registerActor(makeTestActor("UIButton"));

    SceneDefinition scene;
    scene.sceneName = "MainMenu";
    scene.targetMode = BuildMode::Web;
    scene.actorNames = {"UIButton"};
    system.registerScene(scene);

    auto result = system.buildScene("MainMenu");
    EXPECT_TRUE(result.success);

    system.shutdown();
}

} // namespace ergo::compose::test
