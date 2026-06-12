#pragma once

/// ergo::character — Control 層 (向き) のインターフェース。
///
/// TCC の `ITurn` + `IPriorityLifecycle<ITurn>` に対応する。最高優先度の
/// 1 つが採用され、Brain が `target_yaw_deg()` へ `turn_speed_deg()` (deg/s)
/// で最短弧補間する。turn_speed_deg() が負なら即時スナップ。
///
/// Spec: spec/module/character.md

namespace ergo::character {

class ITurnSource {
public:
    virtual ~ITurnSource() = default;

    /// 優先度。0 以下は「存在しない」扱い (TCC 互換)。
    virtual int turn_priority() const = 0;

    /// 補間速度 (deg/s)。負値で即時スナップ。
    virtual float turn_speed_deg() const = 0;

    /// 最終的に向きたい yaw (deg、Z+ が 0 度)。
    virtual float target_yaw_deg() const = 0;

    // ── lifecycle フック (任意 override) ──────────────────────────────
    virtual void on_turn_acquire_highest() {}
    virtual void on_turn_lose_highest() {}
    virtual void on_turn_update_highest(float /*dt*/) {}
};

} // namespace ergo::character
