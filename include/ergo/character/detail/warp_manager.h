#pragma once

/// ergo::character::detail — warp (位置・向きの強制上書き) の保留管理。
///
/// TCC の `WarpManager` に対応する。warp は Control / Effect より優先され、
/// フレーム末尾 (Brain 反映後) にクリアされる。BrainBase の内部実装。
///
/// Spec: spec/module/character.md

#include "ergo/character/types.h"

namespace ergo::character::detail {

class WarpManager {
public:
    void set_position(const Vec3& p) { position_ = p; warped_position_ = true; }
    void set_yaw(float yaw_deg) { yaw_deg_ = yaw_deg; warped_yaw_ = true; }

    bool warped_position() const { return warped_position_; }
    bool warped_yaw() const { return warped_yaw_; }
    const Vec3& position() const { return position_; }
    float yaw_deg() const { return yaw_deg_; }

    void reset() { warped_position_ = false; warped_yaw_ = false; }

private:
    Vec3  position_{};
    float yaw_deg_ = 0.0f;
    bool  warped_position_ = false;
    bool  warped_yaw_      = false;
};

} // namespace ergo::character::detail
