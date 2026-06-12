#pragma once

/// ergo::character — Control 層 (移動) のインターフェース。
///
/// TCC の `IMove` + `IPriorityLifecycle<IMove>` に対応する。複数の
/// IMoveSource を Brain に登録し、`move_priority() > 0` のうち最高優先度の
/// 1 つだけが採用される (加算ではない。加算したい速度は IEffectSource へ)。
///
/// Spec: spec/module/character.md

#include "ergo/character/types.h"

namespace ergo::character {

class IMoveSource {
public:
    virtual ~IMoveSource() = default;

    /// 優先度。0 以下は「存在しない」扱いで採用候補から外れる (TCC 互換)。
    virtual int move_priority() const = 0;

    /// このフレームの希望移動速度 (ワールド、unit/s)。
    virtual Vec3 move_velocity() const = 0;

    // ── lifecycle フック (任意 override。TCC IPriorityLifecycle 相当) ──
    /// 最高優先度を獲得した直後に 1 回呼ばれる。
    virtual void on_move_acquire_highest() {}
    /// 最高優先度を失った直後に 1 回呼ばれる。
    virtual void on_move_lose_highest() {}
    /// 最高優先度を保持している間、毎フレーム呼ばれる。
    virtual void on_move_update_highest(float /*dt*/) {}
};

} // namespace ergo::character
