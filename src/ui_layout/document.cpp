#include "ergo/ui_layout/ui_layout.h"

#include <algorithm>

namespace ergo::ui_layout {

void Document::set_viewport(int w, int h) {
    viewport_w_ = std::max(1, w);
    viewport_h_ = std::max(1, h);
}

void Document::update(const BindContext& ctx, float /*dt*/) {
    apply_binds_(root_, ctx);
    solve_layout_();
}

void Document::emit(RenderAdapter& adapter) {
    emit_node_(root_, adapter);
}

Node* Document::find(std::string_view id) {
    return find_node_(root_, id);
}

Node* Document::hit_test(float x, float y) {
    return hit_test_node_(root_, x, y);
}

} // namespace ergo::ui_layout
