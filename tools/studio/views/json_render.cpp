#include "json_render.h"

#include <cstdio>

namespace ergo::studio::views {
namespace jm = ergo::common::jsonm;

std::string scalar_text(const JsonValue& v) {
    if (v.is_string()) return v.s;
    if (v.is_bool()) return v.b ? "true" : "false";
    if (v.is_null()) return "null";
    if (v.is_number()) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.15g", v.n);
        return buf;
    }
    return jm::serialize(v);
}

std::string text_of(const JsonValue& v, const char* key) {
    const JsonValue* f = v.find(key);
    return f && !f->is_null() ? scalar_text(*f) : std::string();
}

namespace {

bool is_container(const JsonValue& v) { return v.is_array() || v.is_object(); }

/// First line of a scalar (the explorer is not a text viewer).
std::string one_line(const std::string& s) {
    const auto nl = s.find('\n');
    return nl == std::string::npos ? s : s.substr(0, nl) + " \xE2\x80\xA6";
}

void render_value(ui::ViewBuilder& b, const std::string& key, const JsonValue& v, const JsonRenderLimits& lim, int depth);

void render_children(ui::ViewBuilder& b, const JsonValue& v, const JsonRenderLimits& lim, int depth) {
    if (v.is_object()) {
        for (const auto& [k, child] : *v.o) render_value(b, k, child, lim, depth);
        return;
    }
    std::size_t i = 0;
    for (const auto& child : *v.a) {
        if (i == lim.max_items) {
            b.muted("\xE2\x80\xA6 " + std::to_string(v.a->size() - i) + " more");
            break;
        }
        render_value(b, "[" + std::to_string(i) + "]", child, lim, depth);
        ++i;
    }
}

void render_value(ui::ViewBuilder& b, const std::string& key, const JsonValue& v, const JsonRenderLimits& lim, int depth) {
    if (!is_container(v)) {
        ui::ViewBuilder r = b.row(6);
        if (!key.empty()) r.label(key + ":", &b.theme().muted);
        r.clipped_label(one_line(scalar_text(v)), lim.max_width);
        return;
    }
    const std::size_t n = v.is_object() ? v.o->size() : v.a->size();
    const std::string head = key + (v.is_object() ? "  {" : "  [") + std::to_string(n) + (v.is_object() ? "}" : "]");
    if (depth >= lim.max_depth || n == 0) {
        b.muted(head + (n == 0 ? "" : "  \xE2\x80\xA6"));
        return;
    }
    if (!key.empty()) b.muted(head);
    ui::ViewBuilder p = b.panel(3);
    render_children(p, v, lim, depth + 1);
}

} // namespace

void render_json(ui::ViewBuilder& b, const JsonValue& v, const JsonRenderLimits& lim) {
    if (is_container(v)) {
        ui::ViewBuilder p = b.panel(3);
        render_children(p, v, lim, 1);
    } else {
        render_value(b, "", v, lim, 0);
    }
}

} // namespace ergo::studio::views
