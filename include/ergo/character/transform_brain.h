#pragma once

/// ergo::character — TransformBrain: ホスト所有 Pose への直接書込み Brain。
///
/// TCC の `TransformBrain` に対応する。物理エンジンを介さず、
/// `pose->position += total_velocity * dt` で位置を進める。衝突解決が
/// 必要なホストは `set_move_filter` で delta を加工する (壁ずり・クランプ
/// など)。filter は通常移動にのみ適用され、warp には適用されない。
///
/// Spec: spec/module/character.md

#include "ergo/character/brain.h"

#include <functional>
#include <utility>

namespace ergo::character {

class TransformBrain : public BrainBase {
public:
    /// 移動 delta の加工関数。(現在位置, 希望 delta) → 実際に動かす delta。
    using MoveFilter = std::function<Vec3(const Vec3& position, const Vec3& delta)>;

    /// `pose` はホスト所有 (本クラスより長寿命であること)。
    explicit TransformBrain(Pose* pose);

    /// 衝突解決などの delta 加工をホストから注入する (任意)。
    void set_move_filter(MoveFilter filter) { move_filter_ = std::move(filter); }

protected:
    void apply_position(const Vec3& total_velocity, float dt) override;
    void apply_yaw(float yaw_deg) override;
    void set_position_directly(const Vec3& position) override;
    void set_yaw_directly(float yaw_deg) override;

private:
    Pose*      pose_;
    MoveFilter move_filter_;
};

} // namespace ergo::character
