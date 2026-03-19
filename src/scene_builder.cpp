#include "ergo/compose/scene_builder.h"

#include <sstream>

namespace ergo::compose {

SceneBuilder::SceneBuilder() = default;
SceneBuilder::~SceneBuilder() = default;

void SceneBuilder::setActorComposer(ActorComposer* composer) {
    composer_ = composer;
}

void SceneBuilder::registerScene(const SceneDefinition& scene) {
    scenes_[scene.sceneName] = scene;
}

void SceneBuilder::unregisterScene(const std::string& sceneName) {
    scenes_.erase(sceneName);
}

const SceneDefinition* SceneBuilder::getScene(const std::string& sceneName) const {
    auto it = scenes_.find(sceneName);
    return (it != scenes_.end()) ? &it->second : nullptr;
}

BuildResult SceneBuilder::buildScene(const std::string& sceneName) {
    auto it = scenes_.find(sceneName);
    if (it == scenes_.end()) {
        BuildResult result;
        result.success = false;
        result.errorMessage = "Scene not found: " + sceneName;
        return result;
    }

    const auto& scene = it->second;

    switch (scene.targetMode) {
        case BuildMode::Web:
            return buildForWeb(sceneName);
        case BuildMode::App:
            return buildForApp(sceneName);
    }

    BuildResult result;
    result.success = false;
    result.errorMessage = "Unknown build mode";
    return result;
}

BuildResult SceneBuilder::buildForWeb(const std::string& sceneName) {
    BuildResult result;
    auto startTime = std::chrono::steady_clock::now();

    auto it = scenes_.find(sceneName);
    if (it == scenes_.end()) {
        result.success = false;
        result.errorMessage = "Scene not found: " + sceneName;
        return result;
    }

    if (!composer_) {
        result.success = false;
        result.errorMessage = "ActorComposer not set";
        return result;
    }

    const auto& scene = it->second;

    // 全アクターをWeb向けにビルド
    std::vector<BuildResult> actorResults;
    for (const auto& actorName : scene.actorNames) {
        auto actorResult = composer_->buildActor(actorName, BuildMode::Web);
        if (!actorResult.success) {
            result.success = false;
            result.errorMessage = "Actor build failed: " + actorName +
                                  " - " + actorResult.errorMessage;
            return result;
        }
        actorResults.push_back(std::move(actorResult));
    }

    // Web成果物を生成
    result = generateWebArtifacts(scene, actorResults);

    auto endTime = std::chrono::steady_clock::now();
    result.buildTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    return result;
}

BuildResult SceneBuilder::buildForApp(const std::string& sceneName) {
    BuildResult result;
    auto startTime = std::chrono::steady_clock::now();

    auto it = scenes_.find(sceneName);
    if (it == scenes_.end()) {
        result.success = false;
        result.errorMessage = "Scene not found: " + sceneName;
        return result;
    }

    if (!composer_) {
        result.success = false;
        result.errorMessage = "ActorComposer not set";
        return result;
    }

    const auto& scene = it->second;

    // 全アクターをApp向けにビルド
    std::vector<BuildResult> actorResults;
    for (const auto& actorName : scene.actorNames) {
        auto actorResult = composer_->buildActor(actorName, BuildMode::App);
        if (!actorResult.success) {
            result.success = false;
            result.errorMessage = "Actor build failed: " + actorName +
                                  " - " + actorResult.errorMessage;
            return result;
        }
        actorResults.push_back(std::move(actorResult));
    }

    // App成果物を生成
    result = generateAppArtifacts(scene, actorResults);

    auto endTime = std::chrono::steady_clock::now();
    result.buildTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    return result;
}

void SceneBuilder::setOutputPath(const std::string& path) {
    outputPath_ = path;
}

BuildResult SceneBuilder::generateWebArtifacts(
    const SceneDefinition& scene,
    const std::vector<BuildResult>& actorResults) {

    BuildResult result;

    // WebGL + Wasm + TS/JS 構成のビルド成果物を生成
    // - ErgoモジュールはWasmにコンパイル
    // - ゲームロジックはTS/JSで動作
    // - PictorのWebGLレンダラーを使用

    std::ostringstream manifest;
    manifest << "{\n";
    manifest << "  \"scene\": \"" << scene.sceneName << "\",\n";
    manifest << "  \"platform\": \"web\",\n";
    manifest << "  \"renderer\": \"webgl\",\n";
    manifest << "  \"runtime\": \"wasm+js\",\n";
    manifest << "  \"actors\": [\n";

    for (size_t i = 0; i < actorResults.size(); ++i) {
        manifest << "    {\"name\": \"" << scene.actorNames[i]
                 << "\", \"output\": \"" << actorResults[i].outputPath << "\"}";
        if (i + 1 < actorResults.size()) manifest << ",";
        manifest << "\n";
    }

    manifest << "  ]\n";
    manifest << "}\n";

    result.success = true;
    result.outputPath = outputPath_ + "/" + scene.sceneName + ".web.json";

    return result;
}

BuildResult SceneBuilder::generateAppArtifacts(
    const SceneDefinition& scene,
    const std::vector<BuildResult>& actorResults) {

    BuildResult result;

    // App構成: exe + QUICサブモジュール連携
    // - デバッグ用命令サブモジュールを含む
    // - QUICで双方向プロセス間通信
    // - シーンアクターの再生/停止/テスト/プロファイル

    std::ostringstream manifest;
    manifest << "{\n";
    manifest << "  \"scene\": \"" << scene.sceneName << "\",\n";
    manifest << "  \"platform\": \"app\",\n";
    manifest << "  \"renderer\": \"native\",\n";
    manifest << "  \"runtime\": \"exe+quic\",\n";
    manifest << "  \"quic_enabled\": true,\n";
    manifest << "  \"actors\": [\n";

    for (size_t i = 0; i < actorResults.size(); ++i) {
        manifest << "    {\"name\": \"" << scene.actorNames[i]
                 << "\", \"output\": \"" << actorResults[i].outputPath << "\"}";
        if (i + 1 < actorResults.size()) manifest << ",";
        manifest << "\n";
    }

    manifest << "  ]\n";
    manifest << "}\n";

    result.success = true;
    result.outputPath = outputPath_ + "/" + scene.sceneName + ".app.json";

    return result;
}

} // namespace ergo::compose
