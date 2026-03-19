#pragma once

#include "ergo/compose/types.h"
#include "ergo/compose/actor_composer.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace ergo::compose {

/// シーンビルダー
/// シーン単位でアクターをまとめてビルドし、プレイ確認用の成果物を生成する
/// Web: wasm+js / App: exe連携コマンド
class SceneBuilder {
public:
    SceneBuilder();
    ~SceneBuilder();

    /// ActorComposerを設定
    void setActorComposer(ActorComposer* composer);

    /// シーンを登録
    void registerScene(const SceneDefinition& scene);

    /// シーンを解除
    void unregisterScene(const std::string& sceneName);

    /// シーン定義を取得
    const SceneDefinition* getScene(const std::string& sceneName) const;

    /// シーンをビルド（含まれる全アクターを結合）
    BuildResult buildScene(const std::string& sceneName);

    /// Webビルド（WebGL + Wasm + TS/JS）
    BuildResult buildForWeb(const std::string& sceneName);

    /// Appビルド（exe + QUIC連携）
    BuildResult buildForApp(const std::string& sceneName);

    /// 出力ディレクトリを設定
    void setOutputPath(const std::string& path);

private:
    ActorComposer* composer_ = nullptr;
    std::unordered_map<std::string, SceneDefinition> scenes_;
    std::string outputPath_;

    /// Web向け成果物を生成（内部）
    BuildResult generateWebArtifacts(const SceneDefinition& scene,
                                     const std::vector<BuildResult>& actorResults);

    /// App向け成果物を生成（内部）
    BuildResult generateAppArtifacts(const SceneDefinition& scene,
                                     const std::vector<BuildResult>& actorResults);
};

} // namespace ergo::compose
