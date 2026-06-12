#pragma once

/// ergo::character — Check (センサー) 層のインターフェース。
///
/// TCC の `IEarlyUpdateComponent` に対応する。Brain の集約・反映より先に
/// `update_order` 昇順で `on_update(dt)` が呼ばれ、環境情報 (接地・壁・
/// 天井など) をフレーム内キャッシュする。Control / Effect が同フレームの
/// センサー結果を読めるよう、必ず Brain 反映前に実行される。
///
/// Spec: spec/module/character.md

namespace ergo::character {

class IUpdateComponent {
public:
    virtual ~IUpdateComponent() = default;

    /// 実行順 (昇順)。センサー間に依存があるときに使う。
    virtual int update_order() const = 0;

    /// Brain 反映前の先行更新。
    virtual void on_update(float dt) = 0;
};

} // namespace ergo::character
