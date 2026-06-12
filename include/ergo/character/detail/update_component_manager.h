#pragma once

/// ergo::character::detail — IUpdateComponent (Check 層) の順序付き実行。
///
/// TCC の `UpdateComponentManager` に対応する。BrainBase の内部実装。
///
/// Spec: spec/module/character.md

#include "ergo/character/update_component.h"

#include <vector>

namespace ergo::character::detail {

class UpdateComponentManager {
public:
    void add(IUpdateComponent* component);
    void remove(IUpdateComponent* component);

    /// update_order 昇順で on_update(dt) を呼ぶ。
    void process(float dt);

private:
    std::vector<IUpdateComponent*> components_;  // update_order 昇順を維持
};

} // namespace ergo::character::detail
