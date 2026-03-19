#pragma once

#include "ergo/compose/types.h"
#include "ergo/compose/actor_composer.h"
#include "ergo/compose/scene_builder.h"
#include "ergo/compose/quic_commander.h"
#include "ergo/compose/hot_reloader.h"

namespace ergo::compose {

/// ComposeSystem — コンポーザーシステムのファサード
/// Arsからのコンパイル指令を受け、全サブシステムを統括する
class ComposeSystem {
public:
    ComposeSystem();
    ~ComposeSystem();

    /// システム初期化
    void initialize(const ComposeConfig& config);

    /// システム終了
    void shutdown();

    // ------- アクター管理 -------

    /// アクター結合定義を登録
    void registerActor(const ActorComposition& composition);

    /// アクターをビルド
    BuildResult buildActor(const std::string& actorName);

    // ------- シーン管理 -------

    /// シーンを登録
    void registerScene(const SceneDefinition& scene);

    /// シーンをビルド（プレイ確認用）
    BuildResult buildScene(const std::string& sceneName);

    // ------- Arsコンパイル指令 -------

    /// Arsからのコンパイル指令を処理
    /// アクター/シーン単位のビルドを実行する
    BuildResult processCompileDirective(const std::string& targetName, BuildUnit unit);

    // ------- プラットフォーム切替 -------

    /// ビルドモードを設定
    void setBuildMode(BuildMode mode);

    /// 現在のビルドモードを取得
    BuildMode getBuildMode() const;

    // ------- サブシステムアクセス -------

    ActorComposer&  actorComposer();
    SceneBuilder&   sceneBuilder();
    QuicCommander&  quicCommander();
    HotReloader&    hotReloader();
    Transpiler&     transpiler();

private:
    ComposeConfig   config_;
    BuildMode       currentMode_;
    ActorComposer   actorComposer_;
    SceneBuilder    sceneBuilder_;
    QuicCommander   quicCommander_;
    HotReloader     hotReloader_;
    bool            initialized_ = false;
};

} // namespace ergo::compose
