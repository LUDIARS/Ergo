#include "ergo/character/brain.h"

namespace ergo::character {

void BrainBase::update(float dt) {
    // 処理順は spec/module/character.md「タスク」節 (TCC UpdateBrain 準拠)。

    // 1. Check 層の先行更新
    update_.process(dt);

    // 2. 最高優先度の再評価 + lifecycle
    move_.update_highest(dt);
    turn_.update_highest(dt);

    // 3. 速度・角度の確定
    effect_.calculate_velocity();
    move_.calculate_velocity();
    turn_.calculate_angle(dt, cached_yaw_deg_);

    // 4. 合成
    total_velocity_ = move_.velocity() + effect_.velocity();

    // 5. 位置反映 (warp 最優先 — 位置 warp は Effect をリセットする)
    if (warp_.warped_position()) {
        set_position_directly(warp_.position());
        effect_.reset_all();
    } else {
        apply_position(total_velocity_, dt);
    }

    // 6. 向き反映 (warp 最優先、無ければ採用 Turn がいるときのみ)
    if (warp_.warped_yaw()) {
        set_yaw_directly(warp_.yaw_deg());
    } else if (turn_.has_highest()) {
        apply_yaw(turn_.next_yaw_deg());
    }

    // 7. warp 保留のクリア
    warp_.reset();
}

} // namespace ergo::character
