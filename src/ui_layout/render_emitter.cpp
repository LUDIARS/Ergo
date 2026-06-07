#include "ergo/ui_layout/ui_layout.h"

namespace ergo::ui_layout {

void Document::emit_node_(Node& node, RenderAdapter& adapter) {
    if (!node.visible) return;

    if (node.type == "rect") adapter.draw_rect(node.resolved_rect, node.color.empty() ? "#ffffff" : node.color, node.opacity);
    else if (node.type == "nine_slice") adapter.draw_nine_slice(node.resolved_rect, node.nine_slice, node.opacity);
    else if (node.type == "image") adapter.draw_image(node.image, node.resolved_rect, node.opacity);
    else if (node.type == "text") adapter.draw_text(node.text_value, node.text_style, node.resolved_rect, node.opacity);
    else if (node.type == "vector") {
        node.vector.rect = node.resolved_rect;
        node.vector.color = node.color.empty() ? "#ffffff" : node.color;
        adapter.draw_vector_mesh(node.vector, node.opacity);
    }

    if (node.type == "container") adapter.push_clip(node.resolved_rect);
    for (auto& c : node.children) emit_node_(c, adapter);
    if (node.type == "container") adapter.pop_clip();
}

} // namespace ergo::ui_layout
