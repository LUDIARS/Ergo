#include "ergo/character/detail/update_component_manager.h"

#include <algorithm>

namespace ergo::character::detail {

void UpdateComponentManager::add(IUpdateComponent* component) {
    if (!component) return;
    if (std::find(components_.begin(), components_.end(), component) != components_.end())
        return;
    // update_order 昇順の位置へ挿入する (登録順が同 order 内の安定順)。
    auto it = std::find_if(components_.begin(), components_.end(),
                           [&](IUpdateComponent* c) {
                               return c->update_order() > component->update_order();
                           });
    components_.insert(it, component);
}

void UpdateComponentManager::remove(IUpdateComponent* component) {
    components_.erase(std::remove(components_.begin(), components_.end(), component),
                      components_.end());
}

void UpdateComponentManager::process(float dt) {
    for (IUpdateComponent* c : components_) c->on_update(dt);
}

} // namespace ergo::character::detail
