#pragma once

/// ergo::character::detail — IEffectSource の合算。
///
/// TCC の `EffectManager` に対応する。BrainBase の内部実装。
///
/// Spec: spec/module/character.md

#include "ergo/character/effect_source.h"

#include <vector>

namespace ergo::character::detail {

class EffectManager {
public:
    void add(IEffectSource* source);
    void remove(IEffectSource* source);

    /// 全 Effect の速度を加算して確定する。
    void calculate_velocity();

    /// 全 Effect の速度を強制リセットする (位置 warp 時)。
    void reset_all();

    const Vec3& velocity() const { return velocity_; }

private:
    std::vector<IEffectSource*> sources_;
    Vec3 velocity_{};
};

} // namespace ergo::character::detail
