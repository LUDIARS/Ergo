#pragma once

/// ergo::character — BrainBase: コンポーネント出力の集約 + 最終反映。
///
/// TCC の `BrainBase` に対応する。Control (最高優先度 1 つ) と Effect
/// (全加算) を合成して total_velocity を作り、warp を最優先で適用し、
/// 位置・向きの実際の書込みは派生 Brain (`apply_position` 等) に委ねる。
///
/// Unity の GetComponents 相当の自動収集はしない — ホストが add_* で明示
/// 登録する (登録解除しないままコンポーネントを破棄しないこと)。
///
/// Spec: spec/module/character.md

#include "ergo/character/detail/effect_manager.h"
#include "ergo/character/detail/move_manager.h"
#include "ergo/character/detail/turn_manager.h"
#include "ergo/character/detail/update_component_manager.h"
#include "ergo/character/detail/warp_manager.h"
#include "ergo/character/types.h"

namespace ergo::character {

class BrainBase {
public:
    virtual ~BrainBase() = default;

    // ── コンポーネント登録 ────────────────────────────────────────────
    void add_move(IMoveSource* s) { move_.add(s); }
    void remove_move(IMoveSource* s) { move_.remove(s); }
    void add_turn(ITurnSource* s) { turn_.add(s); }
    void remove_turn(ITurnSource* s) { turn_.remove(s); }
    void add_effect(IEffectSource* s) { effect_.add(s); }
    void remove_effect(IEffectSource* s) { effect_.remove(s); }
    void add_update(IUpdateComponent* c) { update_.add(c); }
    void remove_update(IUpdateComponent* c) { update_.remove(c); }

    // ── フレーム更新 (spec の処理順を参照) ────────────────────────────
    void update(float dt);

    // ── warp (Control / Effect より優先) ──────────────────────────────
    void warp(const Vec3& position) { warp_.set_position(position); }
    void warp_yaw(float yaw_deg) { warp_.set_yaw(yaw_deg); }
    void warp(const Vec3& position, float yaw_deg) {
        warp_.set_position(position);
        warp_.set_yaw(yaw_deg);
    }

    // ── 状態アクセサ ──────────────────────────────────────────────────
    /// Control 由来の速度 (最高優先度の IMoveSource 1 つ)。
    const Vec3& control_velocity() const { return move_.velocity(); }
    /// Effect 由来の加算速度 (全 IEffectSource の合算)。
    const Vec3& effect_velocity() const { return effect_.velocity(); }
    /// control + effect。位置反映に使われた最終速度。
    const Vec3& total_velocity() const { return total_velocity_; }
    /// Control 由来の速さ (unit/s)。採用 source が無ければ 0。
    float current_speed() const { return move_.current_speed(); }

    /// 現在のワールド位置 / 向き (派生 Brain が書き戻したキャッシュ)。
    const Vec3& position() const { return cached_position_; }
    float yaw_deg() const { return cached_yaw_deg_; }

    /// 採用中 Turn の目標 yaw / 現在向きとの最短弧差 (deg)。
    float target_yaw_deg() const { return turn_.target_yaw_deg(); }
    float delta_turn_deg() const { return turn_.delta_turn_deg(); }

protected:
    // ── 派生 Brain が実装する反映層 ───────────────────────────────────
    /// 通常移動の反映。total_velocity * dt を位置に足す (衝突解決は派生側)。
    virtual void apply_position(const Vec3& total_velocity, float dt) = 0;
    /// 補間後 yaw の反映。
    virtual void apply_yaw(float yaw_deg) = 0;
    /// warp による位置の直接セット (Control / Effect の影響を受けない)。
    virtual void set_position_directly(const Vec3& position) = 0;
    /// warp による向きの直接セット。
    virtual void set_yaw_directly(float yaw_deg) = 0;

    /// 派生 Brain は反映後に現在値をここへ書き戻す。
    Vec3  cached_position_{};
    float cached_yaw_deg_ = 0.0f;

private:
    detail::MoveManager            move_;
    detail::TurnManager            turn_;
    detail::EffectManager          effect_;
    detail::UpdateComponentManager update_;
    detail::WarpManager            warp_;
    Vec3 total_velocity_{};
};

} // namespace ergo::character
