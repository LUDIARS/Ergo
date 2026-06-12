#include "ergo/character/detail/turn_manager.h"

#include "ergo/character/types.h"

#include <algorithm>

namespace ergo::character::detail {

void TurnManager::add(ITurnSource* source) {
    if (!source) return;
    if (std::find(sources_.begin(), sources_.end(), source) != sources_.end()) return;
    sources_.push_back(source);
}

void TurnManager::remove(ITurnSource* source) {
    sources_.erase(std::remove(sources_.begin(), sources_.end(), source), sources_.end());
    if (current_ == source) current_ = nullptr;
}

void TurnManager::update_highest(float dt) {
    ITurnSource* best = nullptr;
    int best_priority = 0;
    for (ITurnSource* s : sources_) {
        const int p = s->turn_priority();
        if (p > best_priority) {
            best = s;
            best_priority = p;
        }
    }
    if (best != current_) {
        if (current_) current_->on_turn_lose_highest();
        current_ = best;
        if (current_) current_->on_turn_acquire_highest();
    }
    if (current_) current_->on_turn_update_highest(dt);
}

void TurnManager::calculate_angle(float dt, float current_yaw_deg) {
    if (!current_) {
        delta_turn_deg_ = 0.0f;
        return;
    }
    target_yaw_deg_ = current_->target_yaw_deg();
    delta_turn_deg_ = wrap_angle_deg(target_yaw_deg_ - current_yaw_deg);

    const float speed = current_->turn_speed_deg();
    if (speed < 0.0f) {
        next_yaw_deg_ = target_yaw_deg_;  // 負値は即時スナップ
    } else {
        next_yaw_deg_ =
            move_toward_angle_deg(current_yaw_deg, target_yaw_deg_, speed * dt);
    }
}

} // namespace ergo::character::detail
