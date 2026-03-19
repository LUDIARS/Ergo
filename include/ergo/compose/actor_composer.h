#pragma once

#include "ergo/compose/types.h"
#include "ergo/compose/transpiler.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace ergo::compose {

/// アクターコンポーザー
/// アクターに対してロジック・Ergoモジュール・Pictor描画を結合する
class ActorComposer {
public:
    ActorComposer();
    ~ActorComposer();

    /// アクター結合定義を登録
    void registerActor(const ActorComposition& composition);

    /// アクター結合定義を解除
    void unregisterActor(const std::string& actorName);

    /// 登録済みアクターを取得
    const ActorComposition* getActor(const std::string& actorName) const;

    /// 全登録アクター名を取得
    std::vector<std::string> getActorNames() const;

    /// モジュールバインディングを追加
    void addModuleBinding(const std::string& actorName, const ModuleBinding& binding);

    /// 描画バインディングを設定
    void setRenderBinding(const std::string& actorName, const RenderBinding& binding);

    /// アクター単位でビルド
    /// ロジック(TS→C++)・モジュール・描画の結合コードを生成
    BuildResult buildActor(const std::string& actorName, BuildMode mode);

    /// 全アクターをビルド
    std::vector<BuildResult> buildAll(BuildMode mode);

    /// アクターの依存関係を解決（トポロジカルソート）
    std::vector<std::string> resolveDependencies(const std::string& actorName) const;

    /// Transpiler への参照
    Transpiler& transpiler();

private:
    std::unordered_map<std::string, ActorComposition> actors_;
    Transpiler transpiler_;

    /// 結合コード生成（内部）
    std::string generateBindingCode(const ActorComposition& actor, BuildMode mode) const;
};

} // namespace ergo::compose
