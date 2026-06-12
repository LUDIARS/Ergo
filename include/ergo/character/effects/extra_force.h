#pragma once

/// ergo::character — ExtraForce: 減衰付きインパルスの Effect。
///
/// TCC の `ExtraForce` / `AdditionalVelocity` に対応する。ノックバック・
/// 爆風など「一度加わって時間減衰する」速度を表す。Check 層
/// (`IUpdateComponent`) として毎フレーム減衰する。
///
/// Spec: spec/module/character.md

#include "ergo/character/effect_source.h"
#include "ergo/character/update_component.h"

namespace ergo::character {

class ExtraForce : public IEffectSource, public IUpdateComponent {
public:
    /// 速度を即時加算する (ノックバックの起動)。
    void add_impulse(const Vec3& velocity) { velocity_ += velocity; }

    /// 指数減衰係数 (1/s)。大きいほど早く止まる。0 で減衰なし。
    void set_damping(float per_second) { damping_ = per_second; }

    const Vec3& velocity() const { return velocity_; }

    // ── IUpdateComponent ─────────────────────────────────────────────
    int  update_order() const override { return 100; }
    void on_update(float dt) override;

    // ── IEffectSource ────────────────────────────────────────────────
    Vec3 effect_velocity() const override { return velocity_; }
    void reset_effect_velocity() override { velocity_ = {}; }

private:
    Vec3  velocity_{};
    float damping_ = 4.0f;
};

} // namespace ergo::character
