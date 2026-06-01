#include "ergo/ui_layout/ui_layout.h"

namespace ergo::ui_layout {

Node* Document::find_node_(Node& node, std::string_view id) {
    if (node.id == id) return &node;
    for (auto& c : node.children) {
        if (auto* found = find_node_(c, id)) return found;
    }
    return nullptr;
}

Node* Document::hit_test_node_(Node& node, float x, float y) {
    if (!node.visible) return nullptr;
    const auto& r = node.resolved_rect;
    if (x < r.x || y < r.y || x > r.x + r.w || y > r.y + r.h) return nullptr;
    for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
        if (auto* hit = hit_test_node_(*it, x, y)) return hit;
    }
    return &node;
}

} // namespace ergo::ui_layout
