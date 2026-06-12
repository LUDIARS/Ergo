#include "ergo/character/detail/effect_manager.h"

#include <algorithm>

namespace ergo::character::detail {

void EffectManager::add(IEffectSource* source) {
    if (!source) return;
    if (std::find(sources_.begin(), sources_.end(), source) != sources_.end()) return;
    sources_.push_back(source);
}

void EffectManager::remove(IEffectSource* source) {
    sources_.erase(std::remove(sources_.begin(), sources_.end(), source), sources_.end());
}

void EffectManager::calculate_velocity() {
    velocity_ = {};
    for (IEffectSource* s : sources_) velocity_ += s->effect_velocity();
}

void EffectManager::reset_all() {
    for (IEffectSource* s : sources_) s->reset_effect_velocity();
    velocity_ = {};
}

} // namespace ergo::character::detail
