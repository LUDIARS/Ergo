#pragma once

/// ergo::character — Effect 層のインターフェース。
///
/// TCC の `IEffect` に対応する。Control と違い、登録された全 Effect の
/// 速度が **加算** されて total_velocity に乗る (重力・ノックバック・
/// 移動床など)。位置 warp 時には Brain が全 Effect をリセットする。
///
/// Spec: spec/module/character.md

#include "ergo/character/types.h"

namespace ergo::character {

class IEffectSource {
public:
    virtual ~IEffectSource() = default;

    /// このフレームに加算する速度 (ワールド、unit/s)。
    virtual Vec3 effect_velocity() const = 0;

    /// 速度の強制リセット (位置 warp 時に Brain が呼ぶ)。
    virtual void reset_effect_velocity() = 0;
};

} // namespace ergo::character
