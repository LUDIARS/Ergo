#include "ergo/ui_layout/ui_layout.h"

#include "ergo/common/json_min.h"
#include "ergo/io/file.h"

namespace ergo::ui_layout {
namespace jm = ergo::common::jsonm;

namespace {

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
    if (const auto* r = j.find("rect")) out.rect = {jnum(r, "x", 0), jnum(r, "y", 0), jnum(r, "w", 0), jnum(r, "h", 0)};
    out.base_rect = out.rect;  // snapshot authored rect so per-frame scale_x stays idempotent
    if (const auto* a = j.find("anchor")) { out.anchor.h = jstr(a, "h", "left"); out.anchor.v = jstr(a, "v", "top"); }
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
    if (const auto* v = j.find("vector")) { out.vector.source = jstr(v, "src"); out.vector.fit = jstr(v, "fit", "stretch"); out.vector.extrude = jnum(v, "extrude", 0); }
    if (const auto* b = j.find("binds"); b && b->is_array() && b->a) {
        for (const auto& it : *b->a) out.binds.push_back({jstr(&it, "target"), jstr(&it, "op"), jstr(&it, "expr")});
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
    r.set("w", jm::JsonValue::make_number(n.rect.w)); r.set("h", jm::JsonValue::make_number(n.rect.h));
    o.set("rect", std::move(r));
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
    jm::JsonValue ch = jm::JsonValue::make_array();
    for (const auto& c : n.children) ch.push(node_json(c));
    o.set("children", std::move(ch));
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
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) text.erase(0, 3);
    if (!jm::parse(text, root) || !root.is_object()) return nullptr;

    auto doc = std::make_unique<Document>();
    doc->source_json_ = std::move(text);
    doc->schema_version_ = static_cast<int>(root.find("schema_version") ? root.find("schema_version")->as_number(1) : 1);
    doc->name_ = jstr(&root, "name", "uilayout");
    if (const auto* ds = root.find("design_size")) {
        doc->design_w_ = static_cast<int>(jnum(ds, "w", 1280));
        doc->design_h_ = static_cast<int>(jnum(ds, "h", 720));
    }
    const auto* r = root.find("root");
    if (!r) return nullptr;
    parse_node(*r, doc->root_);
    return doc;
}

bool Document::save_file(const std::string& path) const {
    return ergo::io::write_file(path, to_json());
}

std::string Document::to_json() const {
    jm::JsonValue root = jm::JsonValue::make_object();
    root.set("schema_version", jm::JsonValue::make_number(schema_version_));
    root.set("name", jm::JsonValue::make_string(name_));
    jm::JsonValue ds = jm::JsonValue::make_object();
    ds.set("w", jm::JsonValue::make_number(design_w_));
    ds.set("h", jm::JsonValue::make_number(design_h_));
    root.set("design_size", std::move(ds));
    root.set("root", node_json(root_));
    return jm::serialize(root);
}

void Document::apply_patch(std::string_view patch_text) {
    jm::JsonValue p;
    if (!jm::parse(std::string(patch_text), p) || !p.is_object()) return;
    auto* id = p.find("id");
    if (!id || !id->is_string()) return;
    Node* n = find(id->s);
    if (!n) return;

    if (const auto* r = p.find("rect")) {
        n->rect.x = jnum(r, "x", n->rect.x);
        n->rect.y = jnum(r, "y", n->rect.y);
        n->rect.w = jnum(r, "w", n->rect.w);
        n->rect.h = jnum(r, "h", n->rect.h);
        n->base_rect = n->rect;  // editor/live patch redefines the authored base for scale_x
    }
    if (const auto* t = p.find("text")) n->text_value = jstr(t, "value", n->text_value);
    if (const auto* v = p.find("visible")) n->visible = v->as_bool(n->visible);
}

} // namespace ergo::ui_layout
