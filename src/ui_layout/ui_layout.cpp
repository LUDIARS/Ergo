#include "ergo/ui_layout/ui_layout.h"

#include "ergo/common/json_min.h"
#include "ergo/io/file.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ergo::ui_layout {
namespace jm = ergo::common::jsonm;

namespace {

double as_num(const BindValue& v) {
    if (v.kind == BindValue::Kind::Number) return v.number;
    if (v.kind == BindValue::Kind::Bool) return v.boolean ? 1.0 : 0.0;
    if (v.kind == BindValue::Kind::String) {
        try { return std::stod(v.string); } catch (...) { return 0.0; }
    }
    return 0.0;
}

bool as_bool(const BindValue& v) {
    if (v.kind == BindValue::Kind::Bool) return v.boolean;
    if (v.kind == BindValue::Kind::Number) return std::abs(v.number) > 1e-6;
    return !v.string.empty();
}

std::string as_str(const BindValue& v) {
    if (v.kind == BindValue::Kind::String) return v.string;
    if (v.kind == BindValue::Kind::Bool) return v.boolean ? "true" : "false";
    return std::to_string(v.number);
}

std::string trim(std::string s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

BindValue eval_expr(const std::string& expr, const BindContext& ctx) {
    const std::string e = trim(expr);
    const auto q = e.find('?');
    const auto c = e.find(':');
    if (q != std::string::npos && c != std::string::npos && q < c) {
        const auto cond = trim(e.substr(0, q));
        const auto texp = trim(e.substr(q + 1, c - q - 1));
        const auto fexp = trim(e.substr(c + 1));
        return as_bool(eval_expr(cond, ctx)) ? eval_expr(texp, ctx) : eval_expr(fexp, ctx);
    }
    const auto fn = e.find("fmt_mmss(");
    if (fn == 0 && e.back() == ')') {
        const auto arg = trim(e.substr(9, e.size() - 10));
        int secs = static_cast<int>(as_num(eval_expr(arg, ctx)));
        if (secs < 0) secs = 0;
        const int mm = secs / 60;
        const int ss = secs % 60;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
        return BindValue::from_string(buf);
    }
    if (e.rfind("clamp(", 0) == 0 && e.back() == ')') {
        auto args = e.substr(6, e.size() - 7);
        size_t p1 = args.find(','); size_t p2 = args.find(',', p1 + 1);
        if (p1 != std::string::npos && p2 != std::string::npos) {
            const auto v = as_num(eval_expr(args.substr(0, p1), ctx));
            const auto lo = as_num(eval_expr(args.substr(p1 + 1, p2 - p1 - 1), ctx));
            const auto hi = as_num(eval_expr(args.substr(p2 + 1), ctx));
            return BindValue::from_number(std::clamp(v, lo, hi));
        }
    }
    for (const char* op : {"==", "!=", ">=", "<=", ">", "<"}) {
        const auto pos = e.find(op);
        if (pos != std::string::npos) {
            const auto l = as_num(eval_expr(e.substr(0, pos), ctx));
            const auto r = as_num(eval_expr(e.substr(pos + std::strlen(op)), ctx));
            if (std::string(op) == "==") return BindValue::from_bool(std::abs(l - r) < 1e-6);
            if (std::string(op) == "!=") return BindValue::from_bool(std::abs(l - r) >= 1e-6);
            if (std::string(op) == ">=") return BindValue::from_bool(l >= r);
            if (std::string(op) == "<=") return BindValue::from_bool(l <= r);
            if (std::string(op) == ">") return BindValue::from_bool(l > r);
            return BindValue::from_bool(l < r);
        }
    }
    if (e.size() >= 2 && ((e.front() == '\"' && e.back() == '\"') || (e.front() == '\'' && e.back() == '\''))) {
        return BindValue::from_string(e.substr(1, e.size() - 2));
    }
    if (e == "true") return BindValue::from_bool(true);
    if (e == "false") return BindValue::from_bool(false);
    bool numeric = !e.empty() && (std::isdigit(e[0]) || e[0] == '-' || e[0] == '.');
    if (numeric) {
        try { return BindValue::from_number(std::stod(e)); } catch (...) {}
    }
    auto it = ctx.find(e);
    if (it != ctx.end()) return it->second;
    return BindValue::from_number(0.0);
}

float jnum(const jm::JsonValue* o, const char* k, float fb) {
    if (!o) return fb;
    const auto* v = o->find(k);
    return v ? static_cast<float>(v->as_number(fb)) : fb;
}
std::string jstr(const jm::JsonValue* o, const char* k, const std::string& fb = {}) {
    if (!o) return fb;
    const auto* v = o->find(k);
    return (v && v->is_string()) ? v->s : fb;
}
bool jbool(const jm::JsonValue* o, const char* k, bool fb = false) {
    if (!o) return fb;
    const auto* v = o->find(k);
    return v ? v->as_bool(fb) : fb;
}

void parse_node(const jm::JsonValue& j, Node& out) {
    out.id = jstr(&j, "id");
    out.type = jstr(&j, "type", "container");
    out.layout = jstr(&j, "layout", "absolute");
    if (const auto* r = j.find("rect")) {
        out.rect = {jnum(r, "x", 0), jnum(r, "y", 0), jnum(r, "w", 0), jnum(r, "h", 0)};
    }
    if (const auto* a = j.find("anchor")) {
        out.anchor.h = jstr(a, "h", "left"); out.anchor.v = jstr(a, "v", "top");
    }
    if (const auto* s = j.find("stretch")) {
        if (const auto* v = s->find("left")) { out.stretch.has_left = !v->is_null(); out.stretch.left = static_cast<float>(v->as_number(0)); }
        if (const auto* v = s->find("right")) { out.stretch.has_right = !v->is_null(); out.stretch.right = static_cast<float>(v->as_number(0)); }
        if (const auto* v = s->find("top")) { out.stretch.has_top = !v->is_null(); out.stretch.top = static_cast<float>(v->as_number(0)); }
        if (const auto* v = s->find("bottom")) { out.stretch.has_bottom = !v->is_null(); out.stretch.bottom = static_cast<float>(v->as_number(0)); }
    }
    out.visible = jbool(&j, "visible", true);
    out.opacity = jnum(&j, "opacity", 1.0f);
    if (const auto* t = j.find("text")) {
        out.text_style.font = jstr(t, "font"); out.text_style.size = jnum(t, "size", 16);
        out.text_style.align = jstr(t, "align", "left"); out.text_style.color = jstr(t, "color", "#ffffff");
        out.text_value = jstr(t, "value", out.text_value);
    }
    if (const auto* i = j.find("image")) out.image.path = jstr(i, "src");
    if (const auto* n = j.find("nine_slice")) out.nine_slice.path = jstr(n, "src");
    if (const auto* v = j.find("vector")) {
        out.vector.source = jstr(v, "src"); out.vector.fit = jstr(v, "fit", "stretch"); out.vector.extrude = jnum(v, "extrude", 0);
    }
    if (const auto* b = j.find("binds"); b && b->is_array() && b->a) {
        for (const auto& it : *b->a) {
            BindRule r{jstr(&it, "target"), jstr(&it, "op"), jstr(&it, "expr")};
            out.binds.push_back(std::move(r));
        }
    }
    if (const auto* ch = j.find("children"); ch && ch->is_array() && ch->a) {
        for (const auto& c : *ch->a) { Node n; parse_node(c, n); out.children.push_back(std::move(n)); }
    }
}

jm::JsonValue node_json(const Node& n) {
    jm::JsonValue o = jm::JsonValue::make_object();
    o.set("id", jm::JsonValue::make_string(n.id));
    o.set("type", jm::JsonValue::make_string(n.type));
    o.set("layout", jm::JsonValue::make_string(n.layout));
    jm::JsonValue r = jm::JsonValue::make_object();
    r.set("x", jm::JsonValue::make_number(n.rect.x)); r.set("y", jm::JsonValue::make_number(n.rect.y));
    r.set("w", jm::JsonValue::make_number(n.rect.w)); r.set("h", jm::JsonValue::make_number(n.rect.h)); o.set("rect", std::move(r));
    jm::JsonValue a = jm::JsonValue::make_object(); a.set("h", jm::JsonValue::make_string(n.anchor.h)); a.set("v", jm::JsonValue::make_string(n.anchor.v)); o.set("anchor", std::move(a));
    jm::JsonValue s = jm::JsonValue::make_object();
    s.set("left", n.stretch.has_left ? jm::JsonValue::make_number(n.stretch.left) : jm::JsonValue::make_null());
    s.set("right", n.stretch.has_right ? jm::JsonValue::make_number(n.stretch.right) : jm::JsonValue::make_null());
    s.set("top", n.stretch.has_top ? jm::JsonValue::make_number(n.stretch.top) : jm::JsonValue::make_null());
    s.set("bottom", n.stretch.has_bottom ? jm::JsonValue::make_number(n.stretch.bottom) : jm::JsonValue::make_null());
    o.set("stretch", std::move(s));
    if (n.type == "text") {
        jm::JsonValue t = jm::JsonValue::make_object();
        t.set("font", jm::JsonValue::make_string(n.text_style.font));
        t.set("size", jm::JsonValue::make_number(n.text_style.size));
        t.set("align", jm::JsonValue::make_string(n.text_style.align));
        t.set("color", jm::JsonValue::make_string(n.text_style.color));
        t.set("value", jm::JsonValue::make_string(n.text_value));
        o.set("text", std::move(t));
    }
    if (n.type == "vector") {
        jm::JsonValue v = jm::JsonValue::make_object();
        v.set("src", jm::JsonValue::make_string(n.vector.source));
        v.set("fit", jm::JsonValue::make_string(n.vector.fit));
        v.set("extrude", jm::JsonValue::make_number(n.vector.extrude));
        o.set("vector", std::move(v));
    }
    jm::JsonValue bs = jm::JsonValue::make_array();
    for (const auto& b : n.binds) { jm::JsonValue x = jm::JsonValue::make_object(); x.set("target", jm::JsonValue::make_string(b.target)); x.set("op", jm::JsonValue::make_string(b.op)); x.set("expr", jm::JsonValue::make_string(b.expr)); bs.push(std::move(x)); }
    o.set("binds", std::move(bs));
    jm::JsonValue ch = jm::JsonValue::make_array(); for (const auto& c : n.children) ch.push(node_json(c)); o.set("children", std::move(ch));
    return o;
}

} // namespace

std::unique_ptr<Document> Document::load_file(const std::string& path) {
    std::string text;
    if (!ergo::io::read_file(path, text)) return nullptr;
    return load_json(text);
}

std::unique_ptr<Document> Document::load_json(std::string_view json) {
    jm::JsonValue root;
    std::string text(json);
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    if (!jm::parse(text, root) || !root.is_object()) return nullptr;
    auto doc = std::make_unique<Document>();
    doc->source_json_ = std::move(text);
    doc->schema_version_ = static_cast<int>(root.find("schema_version") ? root.find("schema_version")->as_number(1) : 1);
    doc->name_ = jstr(&root, "name", "uilayout");
    if (const auto* ds = root.find("design_size")) {
        doc->design_w_ = static_cast<int>(jnum(ds, "w", 1280));
        doc->design_h_ = static_cast<int>(jnum(ds, "h", 720));
    }
    if (const auto* r = root.find("root")) parse_node(*r, doc->root_); else return nullptr;
    return doc;
}

bool Document::save_file(const std::string& path) const { return ergo::io::write_file(path, to_json()); }

void Document::set_viewport(int w, int h) { viewport_w_ = std::max(1, w); viewport_h_ = std::max(1, h); }

void Document::update(const BindContext& ctx, float /*dt*/) {
    apply_binds_(root_, ctx);
    solve_layout_();
}

void Document::emit(RenderAdapter& adapter) { emit_node_(root_, adapter); }

Node* Document::find(std::string_view id) { return find_node_(root_, id); }

void Document::apply_patch(std::string_view json_merge_patch) {
    jm::JsonValue p;
    if (!jm::parse(std::string(json_merge_patch), p) || !p.is_object()) return;
    auto* id = p.find("id");
    if (!id || !id->is_string()) return;
    Node* n = find(id->s);
    if (!n) return;
    if (const auto* r = p.find("rect")) {
        n->rect.x = jnum(r, "x", n->rect.x); n->rect.y = jnum(r, "y", n->rect.y);
        n->rect.w = jnum(r, "w", n->rect.w); n->rect.h = jnum(r, "h", n->rect.h);
    }
    if (const auto* t = p.find("text")) n->text_value = jstr(t, "value", n->text_value);
    if (const auto* v = p.find("visible")) n->visible = v->as_bool(n->visible);
}

std::string Document::to_json() const {
    jm::JsonValue root = jm::JsonValue::make_object();
    root.set("schema_version", jm::JsonValue::make_number(schema_version_));
    root.set("name", jm::JsonValue::make_string(name_));
    jm::JsonValue ds = jm::JsonValue::make_object(); ds.set("w", jm::JsonValue::make_number(design_w_)); ds.set("h", jm::JsonValue::make_number(design_h_));
    root.set("design_size", std::move(ds));
    root.set("root", node_json(root_));
    return jm::serialize(root);
}

Node* Document::hit_test(float x, float y) { return hit_test_node_(root_, x, y); }

void Document::solve_layout_() {
    const float sx = static_cast<float>(viewport_w_) / static_cast<float>(std::max(1, design_w_));
    const float sy = static_cast<float>(viewport_h_) / static_cast<float>(std::max(1, design_h_));
    Rect root = {0, 0, root_.rect.w > 0 ? root_.rect.w * sx : static_cast<float>(viewport_w_), root_.rect.h > 0 ? root_.rect.h * sy : static_cast<float>(viewport_h_)};
    root_.resolved_rect = root;
    solve_node_(root_, root);
}

void Document::solve_node_(Node& node, const Rect& parent) {
    if (&node != &root_) {
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
    for (auto& c : node.children) {
        if (node.layout == "row") {
            c.resolved_rect = {node.resolved_rect.x + cursor + c.rect.x, node.resolved_rect.y + c.rect.y, c.rect.w, c.rect.h};
            cursor += c.rect.w;
        } else if (node.layout == "column") {
            c.resolved_rect = {node.resolved_rect.x + c.rect.x, node.resolved_rect.y + cursor + c.rect.y, c.rect.w, c.rect.h};
            cursor += c.rect.h;
        }
        solve_node_(c, node.resolved_rect);
    }
}

void Document::apply_binds_(Node& node, const BindContext& ctx) {
    for (const auto& b : node.binds) {
        const BindValue v = eval_expr(b.expr, ctx);
        if (b.op == "text" && b.target == "self") node.text_value = as_str(v);
        else if (b.op == "scale_x") node.rect.w = node.rect.w * static_cast<float>(as_num(v));
        else if (b.op == "opacity") node.opacity = static_cast<float>(as_num(v));
        else if (b.op == "visible") node.visible = as_bool(v);
        else if ((b.op == "color" || b.op == "fill_color") && v.kind == BindValue::Kind::String) node.color = v.string;
    }
    for (auto& c : node.children) apply_binds_(c, ctx);
}

void Document::emit_node_(Node& node, RenderAdapter& adapter) {
    if (!node.visible) return;
    if (node.type == "rect") adapter.draw_rect(node.resolved_rect, node.color.empty() ? "#ffffff" : node.color, node.opacity);
    else if (node.type == "nine_slice") adapter.draw_nine_slice(node.resolved_rect, node.nine_slice, node.opacity);
    else if (node.type == "image") adapter.draw_image(node.image, node.resolved_rect, node.opacity);
    else if (node.type == "text") adapter.draw_text(node.text_value, node.text_style, node.resolved_rect, node.opacity);
    else if (node.type == "vector") { node.vector.rect = node.resolved_rect; adapter.draw_vector_mesh(node.vector, node.opacity); }

    if (node.type == "container") adapter.push_clip(node.resolved_rect);
    for (auto& c : node.children) emit_node_(c, adapter);
    if (node.type == "container") adapter.pop_clip();
}

Node* Document::find_node_(Node& node, std::string_view id) {
    if (node.id == id) return &node;
    for (auto& c : node.children) if (auto* f = find_node_(c, id)) return f;
    return nullptr;
}

Node* Document::hit_test_node_(Node& node, float x, float y) {
    if (!node.visible) return nullptr;
    const auto& r = node.resolved_rect;
    if (x < r.x || y < r.y || x > r.x + r.w || y > r.y + r.h) return nullptr;
    for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) if (auto* f = hit_test_node_(*it, x, y)) return f;
    return &node;
}

} // namespace ergo::ui_layout
