#include "ergo/compose/compose_system.h"

namespace ergo::compose {

ComposeSystem::ComposeSystem()
    : currentMode_(BuildMode::App) {
}

ComposeSystem::~ComposeSystem() {
    if (initialized_) {
        shutdown();
    }
}

void ComposeSystem::initialize(const ComposeConfig& config) {
    config_ = config;
    currentMode_ = config.defaultMode;

    // Transpiler設定
    actorComposer_.transpiler().setTypeDefPath(config.typeDefPath);

    // SceneBuilder設定
    sceneBuilder_.setActorComposer(&actorComposer_);
    sceneBuilder_.setOutputPath(config.outputPath);

    // HotReloader設定
    if (config.hotReloadEnabled) {
        hotReloader_.setSpeculativeConfig(config.speculativeCompile);
    }

    initialized_ = true;
}

void ComposeSystem::shutdown() {
    hotReloader_.stop();
    quicCommander_.disconnect();
    initialized_ = false;
}

void ComposeSystem::registerActor(const ActorComposition& composition) {
    actorComposer_.registerActor(composition);

    // ホットリロード対象として登録
    if (config_.hotReloadEnabled) {
        for (const auto& source : composition.logicSources) {
            hotReloader_.addWatchTarget(source.filePath, composition.actorName);
        }
    }
}

BuildResult ComposeSystem::buildActor(const std::string& actorName) {
    return actorComposer_.buildActor(actorName, currentMode_);
}

void ComposeSystem::registerScene(const SceneDefinition& scene) {
    sceneBuilder_.registerScene(scene);
}

BuildResult ComposeSystem::buildScene(const std::string& sceneName) {
    return sceneBuilder_.buildScene(sceneName);
}

BuildResult ComposeSystem::processCompileDirective(const std::string& targetName, BuildUnit unit) {
    switch (unit) {
        case BuildUnit::Actor:
            return buildActor(targetName);
        case BuildUnit::Scene:
            return buildScene(targetName);
    }

    BuildResult result;
    result.success = false;
    result.errorMessage = "Unknown build unit";
    return result;
}

void ComposeSystem::setBuildMode(BuildMode mode) {
    currentMode_ = mode;
}

BuildMode ComposeSystem::getBuildMode() const {
    return currentMode_;
}

ActorComposer& ComposeSystem::actorComposer() {
    return actorComposer_;
}

SceneBuilder& ComposeSystem::sceneBuilder() {
    return sceneBuilder_;
}

QuicCommander& ComposeSystem::quicCommander() {
    return quicCommander_;
}

HotReloader& ComposeSystem::hotReloader() {
    return hotReloader_;
}

Transpiler& ComposeSystem::transpiler() {
    return actorComposer_.transpiler();
}

} // namespace ergo::compose
