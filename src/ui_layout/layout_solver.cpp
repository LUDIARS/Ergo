#include "ergo/ui_layout/ui_layout.h"

namespace ergo::ui_layout {

namespace {
bool is_row_layout(const std::string& layout) {
    return layout == "row" || layout == "horizontal";
}
bool is_column_layout(const std::string& layout) {
    return layout == "column" || layout == "vertical";
}
} // namespace

void Document::solve_layout_() {
    const float sx = static_cast<float>(viewport_w_) / static_cast<float>(design_w_ > 0 ? design_w_ : 1);
    const float sy = static_cast<float>(viewport_h_) / static_cast<float>(design_h_ > 0 ? design_h_ : 1);
    Rect root = {
        0.0f,
        0.0f,
        root_.rect.w > 0.0f ? root_.rect.w * sx : static_cast<float>(viewport_w_),
        root_.rect.h > 0.0f ? root_.rect.h * sy : static_cast<float>(viewport_h_),
    };
    root_.resolved_rect = root;
    solve_node_(root_, root, false);
}

void Document::solve_node_(Node& node, const Rect& parent, bool preserve_position) {
    if (&node != &root_ && !preserve_position) {
        Rect r = node.rect;
        if (node.stretch.has_left && node.stretch.has_right) {
            r.x = node.stretch.left;
            r.w = parent.w - node.stretch.left - node.stretch.right;
        } else if (node.anchor.h == "center") {
            r.x = (parent.w - r.w) * 0.5f + node.rect.x;
        } else if (node.anchor.h == "right") {
            r.x = parent.w - r.w - node.rect.x;
        }
        if (node.stretch.has_top && node.stretch.has_bottom) {
            r.y = node.stretch.top;
            r.h = parent.h - node.stretch.top - node.stretch.bottom;
        } else if (node.anchor.v == "center") {
            r.y = (parent.h - r.h) * 0.5f + node.rect.y;
        } else if (node.anchor.v == "bottom") {
            r.y = parent.h - r.h - node.rect.y;
        }
        node.resolved_rect = {parent.x + r.x, parent.y + r.y, r.w, r.h};
    }

    float cursor = 0.0f;
    const bool row = is_row_layout(node.layout);
    const bool column = is_column_layout(node.layout);
    for (auto& c : node.children) {
        if (row) {
            c.resolved_rect = {node.resolved_rect.x + cursor + c.rect.x, node.resolved_rect.y + c.rect.y, c.rect.w, c.rect.h};
            cursor += c.rect.w;
        } else if (column) {
            c.resolved_rect = {node.resolved_rect.x + c.rect.x, node.resolved_rect.y + cursor + c.rect.y, c.rect.w, c.rect.h};
            cursor += c.rect.h;
        }
        solve_node_(c, node.resolved_rect, row || column);
    }
}

} // namespace ergo::ui_layout
