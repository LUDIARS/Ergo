#pragma once

/// ergo::character — Gravity: 落下加速の Effect。
///
/// TCC の `Gravity` に対応する。空中では下向きに加速し、接地で停止する。
/// 接地判定は `IGroundContact` を DI で参照する (未設定なら常に空中)。
/// Check 層 (`IUpdateComponent`) として Brain 反映前に毎フレーム積分する。
///
/// Spec: spec/module/character.md

#include "ergo/character/effect_source.h"
#include "ergo/character/ground_contact.h"
#include "ergo/character/update_component.h"

#include <functional>
#include <utility>

namespace ergo::character {

class Gravity : public IEffectSource, public IUpdateComponent {
public:
    /// 接地センサーを注入する (ホスト実装 or ManualGroundCheck)。
    void set_ground_contact(const IGroundContact* gc) { ground_ = gc; }

    void set_gravity(float units_per_s2) { gravity_ = units_per_s2; }
    void set_scale(float scale) { scale_ = scale; }

    /// 上向きの初速を与える (ジャンプ等)。与えた瞬間から空中扱い。
    void add_upward_velocity(float units_per_second);

    /// 現在の落下速度 (下向きが負)。
    float fall_speed() const { return velocity_y_; }
    bool  is_grounded() const { return grounded_; }

    /// 着地 (空中→接地) / 離地 (接地→空中) の通知。
    void on_land(std::function<void(float fall_speed)> f) { on_land_ = std::move(f); }
    void on_leave(std::function<void()> f) { on_leave_ = std::move(f); }

    // ── IUpdateComponent (Check 層で積分) ─────────────────────────────
    int  update_order() const override { return 100; }  // センサー類の後
    void on_update(float dt) override;

    // ── IEffectSource ────────────────────────────────────────────────
    Vec3 effect_velocity() const override { return {0.0f, velocity_y_, 0.0f}; }
    void reset_effect_velocity() override { velocity_y_ = 0.0f; }

private:
    const IGroundContact* ground_ = nullptr;
    float gravity_    = 9.81f;
    float scale_      = 1.0f;
    float velocity_y_ = 0.0f;
    bool  grounded_   = false;
    std::function<void(float)> on_land_;
    std::function<void()>      on_leave_;
};

} // namespace ergo::character
