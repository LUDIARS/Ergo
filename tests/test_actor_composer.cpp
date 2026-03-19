#include <gtest/gtest.h>
#include "ergo/compose/actor_composer.h"

#include <filesystem>
#include <fstream>

namespace ergo::compose::test {

class ActorComposerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "ergo_actor_test";
        std::filesystem::create_directories(testDir_);
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

    ActorComposition makePlayer() {
        ActorComposition actor;
        actor.actorName = "Player";

        TypeScriptSource logic;
        logic.filePath = createTsFile("player_logic.ts", "export class PlayerLogic {}");
        logic.moduleName = "game";
        logic.actorName = "Player";
        actor.logicSources.push_back(logic);

        ModuleBinding inputBinding;
        inputBinding.moduleName = "ergo_input";
        inputBinding.domain = BindingDomain::Input;
        inputBinding.headerPath = "ergo/input/input_system.h";
        actor.moduleBindings.push_back(inputBinding);

        actor.renderBinding.meshId = "player_mesh";
        actor.renderBinding.materialId = "player_material";
        actor.renderBinding.flags = 0x02; // DYNAMIC

        return actor;
    }

    std::filesystem::path testDir_;
    ActorComposer composer_;
};

TEST_F(ActorComposerTest, RegisterAndGetActor) {
    auto player = makePlayer();
    composer_.registerActor(player);

    auto* result = composer_.getActor("Player");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->actorName, "Player");
    EXPECT_EQ(result->moduleBindings.size(), 1u);
}

TEST_F(ActorComposerTest, UnregisterActor) {
    composer_.registerActor(makePlayer());
    composer_.unregisterActor("Player");

    EXPECT_EQ(composer_.getActor("Player"), nullptr);
}

TEST_F(ActorComposerTest, GetActorNames) {
    composer_.registerActor(makePlayer());

    ActorComposition enemy;
    enemy.actorName = "Enemy";
    composer_.registerActor(enemy);

    auto names = composer_.getActorNames();
    EXPECT_EQ(names.size(), 2u);
}

TEST_F(ActorComposerTest, AddModuleBinding) {
    composer_.registerActor(makePlayer());

    ModuleBinding physicsBinding;
    physicsBinding.moduleName = "ergo_physics";
    physicsBinding.domain = BindingDomain::Physics;

    composer_.addModuleBinding("Player", physicsBinding);

    auto* actor = composer_.getActor("Player");
    ASSERT_NE(actor, nullptr);
    EXPECT_EQ(actor->moduleBindings.size(), 2u);
}

TEST_F(ActorComposerTest, SetRenderBinding) {
    composer_.registerActor(makePlayer());

    RenderBinding newBinding;
    newBinding.meshId = "new_mesh";
    newBinding.materialId = "new_material";

    composer_.setRenderBinding("Player", newBinding);

    auto* actor = composer_.getActor("Player");
    ASSERT_NE(actor, nullptr);
    EXPECT_EQ(actor->renderBinding.meshId, "new_mesh");
}

TEST_F(ActorComposerTest, BuildActor_Success) {
    composer_.registerActor(makePlayer());
    auto result = composer_.buildActor("Player", BuildMode::App);
    EXPECT_TRUE(result.success);
}

TEST_F(ActorComposerTest, BuildActor_NotFound) {
    auto result = composer_.buildActor("NonExistent", BuildMode::App);
    EXPECT_FALSE(result.success);
}

TEST_F(ActorComposerTest, BuildAll) {
    composer_.registerActor(makePlayer());

    ActorComposition enemy;
    enemy.actorName = "Enemy";
    composer_.registerActor(enemy);

    auto results = composer_.buildAll(BuildMode::Web);
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(ActorComposerTest, ResolveDependencies) {
    ActorComposition a;
    a.actorName = "A";
    a.dependencies = {"B"};

    ActorComposition b;
    b.actorName = "B";
    b.dependencies = {"C"};

    ActorComposition c;
    c.actorName = "C";

    composer_.registerActor(a);
    composer_.registerActor(b);
    composer_.registerActor(c);

    auto deps = composer_.resolveDependencies("A");
    ASSERT_EQ(deps.size(), 3u);
    // トポロジカル順: C → B → A
    EXPECT_EQ(deps[0], "C");
    EXPECT_EQ(deps[1], "B");
    EXPECT_EQ(deps[2], "A");
}

TEST_F(ActorComposerTest, BuildActor_WebMode) {
    composer_.registerActor(makePlayer());
    auto result = composer_.buildActor("Player", BuildMode::Web);
    EXPECT_TRUE(result.success);
}

TEST_F(ActorComposerTest, BuildActor_AppMode) {
    composer_.registerActor(makePlayer());
    auto result = composer_.buildActor("Player", BuildMode::App);
    EXPECT_TRUE(result.success);
}

} // namespace ergo::compose::test
