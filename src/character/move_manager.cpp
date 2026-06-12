#include "ergo/character/detail/move_manager.h"

#include <algorithm>

namespace ergo::character::detail {

void MoveManager::add(IMoveSource* source) {
    if (!source) return;
    if (std::find(sources_.begin(), sources_.end(), source) != sources_.end()) return;
    sources_.push_back(source);
}

void MoveManager::remove(IMoveSource* source) {
    sources_.erase(std::remove(sources_.begin(), sources_.end(), source), sources_.end());
    if (current_ == source) {
        // lose フックは呼ばない (破棄途中の可能性があるため)。次の
        // update_highest で後継が acquire を受け取る。
        current_ = nullptr;
    }
}

void MoveManager::update_highest(float dt) {
    IMoveSource* best = nullptr;
    int best_priority = 0;  // 0 以下は不在扱い (TCC GetHighestPriority 互換)
    for (IMoveSource* s : sources_) {
        const int p = s->move_priority();
        if (p > best_priority) {
            best = s;
            best_priority = p;
        }
    }
    if (best != current_) {
        if (current_) current_->on_move_lose_highest();
        current_ = best;
        if (current_) current_->on_move_acquire_highest();
    }
    if (current_) current_->on_move_update_highest(dt);
}

void MoveManager::calculate_velocity() {
    velocity_ = current_ ? current_->move_velocity() : Vec3{};
}

} // namespace ergo::character::detail
