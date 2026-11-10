#include "profile_schema.h"

#include "../common/json_pretty.h"
#include "profile_fields.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>

namespace ergo::studio::plugins::render_pipeline {
namespace {

const JsonValue kEmpty = JsonValue::make_object();

const JsonValue& obj_or_empty(const JsonValue* v) { return v && v->is_object() ? *v : kEmpty; }

double as_number(const JsonValue* v, double d) { return v && v->is_number() && std::isfinite(v->n) ? v->n : d; }
bool   as_bool(const JsonValue* v, bool d) { return v && v->is_bool() ? v->b : d; }
std::string as_string(const JsonValue* v, const std::string& d) { return v && v->is_string() ? v->s : d; }

std::string pick(const Choices& c, const JsonValue* v, const std::string& d) {
    if (v && v->is_string() && std::find(c.begin(), c.end(), v->s) != c.end()) return v->s;
    return d;
}

JsonValue string_array(const JsonValue* v, const Choices* allowed = nullptr) {
    JsonValue a = JsonValue::make_array();
    if (!v || !v->is_array()) return a;
    for (const auto& x : *v->a) {
        if (!x.is_string()) continue;
        if (allowed && std::find(allowed->begin(), allowed->end(), x.s) == allowed->end()) continue;
        a.push(jstr(x.s));
    }
    return a;
}

JsonValue numbers(std::initializer_list<double> xs) {
    JsonValue a = JsonValue::make_array();
    for (double x : xs) a.push(jnum(x));
    return a;
}

JsonValue normalize_section(const JsonValue& raw, const Section& s) {
    JsonValue out = JsonValue::make_object();
    for (const Field& f : s.fields) {
        const JsonValue* v = raw.find(f.key);
        switch (f.type) {
            case FieldType::Number: out.set(f.key, jnum(as_number(v, f.num))); break;
            case FieldType::Bool: out.set(f.key, jbool(as_bool(v, f.flag))); break;
            case FieldType::String: out.set(f.key, jstr(as_string(v, f.text))); break;
            case FieldType::Enum: out.set(f.key, jstr(pick(*f.choices, v, f.text))); break;
            case FieldType::StringArray: out.set(f.key, string_array(v)); break;
            case FieldType::Vec3: {
                std::array<double, 3> r = f.vec;
                if (v && v->is_array() && v->a->size() == 3) {
                    for (int i = 0; i < 3; ++i) r[i] = as_number(&(*v->a)[i], f.vec[i]);
                }
                out.set(f.key, numbers({r[0], r[1], r[2]}));
                break;
            }
            case FieldType::Section: out.set(f.key, normalize_section(obj_or_empty(v), *f.section)); break;
        }
    }
    return out;
}

OrderedJson emit_section(const JsonValue& v, const Section& s) {
    OrderedJson o = OrderedJson::object();
    for (const Field& f : s.fields) {
        const JsonValue* x = v.find(f.key);
        if (!x) continue;
        o.add(f.key, f.type == FieldType::Section ? emit_section(*x, *f.section) : OrderedJson::from(*x));
    }
    return o;
}

JsonValue normalize_attachment(const JsonValue& r) {
    JsonValue a = JsonValue::make_object();
    a.set("name", jstr(as_string(r.find("name"), "")));
    const std::string kind   = pick(attachment_kinds(), r.find("kind"), "COLOR");
    const std::string sizing = pick(attachment_sizings(), r.find("sizing"), "SWAPCHAIN_RELATIVE");
    a.set("kind", jstr(kind));
    a.set("format", jstr(pick(attachment_formats(), r.find("format"), "R16G16B16A16_SFLOAT")));
    a.set("sizing", jstr(sizing));
    a.set("usage", string_array(r.find("usage"), &attachment_usage_flags()));
    if (sizing == "SWAPCHAIN_RELATIVE") {
        a.set("scale", jnum(as_number(r.find("scale"), 1.0)));
    } else {
        a.set("width", jnum(as_number(r.find("width"), 0)));
        a.set("height", jnum(as_number(r.find("height"), 0)));
    }
    if (kind == "DEPTH") {
        a.set("clear_depth", jnum(as_number(r.find("clear_depth"), 1.0)));
        a.set("clear_stencil", jnum(as_number(r.find("clear_stencil"), 0)));
    } else {
        const JsonValue* c = r.find("clear_color");
        std::array<double, 4> rgba{0, 0, 0, 1};
        if (c && c->is_array()) {
            for (std::size_t i = 0; i < 4; ++i) rgba[i] = as_number(i < c->a->size() ? &(*c->a)[i] : nullptr, rgba[i]);
        }
        a.set("clear_color", numbers({rgba[0], rgba[1], rgba[2], rgba[3]}));
    }
    return a;
}

OrderedJson emit_attachment(const JsonValue& a) {
    OrderedJson o = OrderedJson::object();
    for (const char* k : {"name", "kind", "format", "sizing", "usage", "scale", "width", "height", "clear_depth",
                          "clear_stencil", "clear_color"}) {
        if (const JsonValue* v = a.find(k)) o.add(k, OrderedJson::from(*v));
    }
    return o;
}

JsonValue normalize_render_pass(const JsonValue& r) {
    JsonValue p = normalize_section(r, render_pass_section());
    JsonValue ops = JsonValue::make_array();
    if (const JsonValue* raw = r.find("attachment_ops"); raw && raw->is_array()) {
        for (const auto& o : *raw->a) ops.push(normalize_section(obj_or_empty(&o), attachment_ops_section()));
    }
    p.set("attachment_ops", ops);
    return p;
}

OrderedJson emit_render_pass(const JsonValue& p) {
    OrderedJson o = emit_section(p, render_pass_section());
    const JsonValue* ops = p.find("attachment_ops");
    if (ops && ops->is_array() && !ops->a->empty()) {
        OrderedJson arr = OrderedJson::array();
        for (const auto& x : *ops->a) arr.push(emit_section(x, attachment_ops_section()));
        o.add("attachment_ops", std::move(arr));
    }
    return o;
}

struct EffectBlock {
    const char*    kind;
    const char*    key;
    const Section* section;
};
const std::vector<EffectBlock>& effect_blocks() {
    static const std::vector<EffectBlock> v{{"Bloom", "bloom", &bloom_section()},
                                           {"ToneMapping", "tone_mapping", &tone_mapping_section()},
                                           {"Vignette", "vignette", &vignette_section()},
                                           {"ColorGrading", "color_grading", &color_grading_section()},
                                           {"DepthOfField", "depth_of_field", &depth_of_field_section()}};
    return v;
}

JsonValue normalize_post_process(const JsonValue& r) {
    JsonValue p = JsonValue::make_object();
    const std::string name = as_string(r.find("name"), "");
    // An explicit, known, non-Unknown kind wins; otherwise infer from the name.
    std::string kind = pick(post_process_kinds(), r.find("kind"), "Unknown");
    if (kind == "Unknown") kind = post_process_kind_from_name(name);
    p.set("name", jstr(name));
    p.set("enabled", jbool(as_bool(r.find("enabled"), true)));
    p.set("kind", jstr(kind));
    for (const auto& b : effect_blocks()) p.set(b.key, normalize_section(obj_or_empty(r.find(b.key)), *b.section));
    return p;
}

OrderedJson emit_post_process(const JsonValue& p) {
    OrderedJson o = OrderedJson::object();
    for (const char* k : {"name", "enabled", "kind"}) o.add(k, OrderedJson::from(*p.find(k)));
    const std::string kind = p.find("kind")->s;
    for (const auto& b : effect_blocks()) {
        if (kind == b.kind) o.add(b.key, emit_section(*p.find(b.key), *b.section));
    }
    return o;
}

/// Number of a JSON value the way JS Number() reads the editor's positions.
std::optional<double> loose_number(const JsonValue* v) {
    if (!v) return std::nullopt;
    if (v->is_number()) return std::isfinite(v->n) ? std::optional<double>(v->n) : std::nullopt;
    if (v->is_string() && !v->s.empty()) {
        char* e = nullptr;
        const double d = std::strtod(v->s.c_str(), &e);
        if (e == v->s.c_str() + v->s.size() && std::isfinite(d)) return d;
    }
    return std::nullopt;
}

/// `_editor` block: only well-formed node positions survive; nullopt when empty.
std::optional<JsonValue> normalize_editor(const JsonValue* raw) {
    if (!raw || !raw->is_object()) return std::nullopt;
    const JsonValue* np = raw->find("nodePositions");
    if (!np || !np->is_object()) return std::nullopt;
    JsonValue pos = JsonValue::make_object();
    for (const auto& [k, v] : *np->o) {
        if (!v.is_object()) continue;
        const auto x = loose_number(v.find("x"));
        const auto y = loose_number(v.find("y"));
        if (x && y) pos.set(k, jobj({{"x", jnum(*x)}, {"y", jnum(*y)}}));
    }
    if (pos.o->empty()) return std::nullopt;
    return jobj({{"nodePositions", pos}});
}

} // namespace

std::string post_process_kind_from_name(const std::string& name) {
    std::string n;
    const auto b = name.find_first_not_of(" \t\r\n");
    if (b != std::string::npos) n = name.substr(b, name.find_last_not_of(" \t\r\n") - b + 1);
    for (char& c : n) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    if (n == "bloom") return "Bloom";
    if (n == "tonemapping" || n == "tonemap" || n == "tone_mapping") return "ToneMapping";
    if (n == "vignette") return "Vignette";
    if (n == "colorgrading" || n == "color_grading" || n == "lut" || n == "grade") return "ColorGrading";
    if (n == "dof" || n == "depthoffield" || n == "depth_of_field") return "DepthOfField";
    return "Unknown";
}

JsonValue default_attachments() {
    JsonValue a = JsonValue::make_array();
    auto usage = [](std::initializer_list<const char*> xs) {
        JsonValue u = JsonValue::make_array();
        for (const char* x : xs) u.push(jstr(x));
        return u;
    };
    a.push(jobj({{"name", jstr("scene_hdr_color")}, {"kind", jstr("COLOR")}, {"format", jstr("R16G16B16A16_SFLOAT")},
                 {"sizing", jstr("SWAPCHAIN_RELATIVE")}, {"scale", jnum(1.0)},
                 {"usage", usage({"COLOR_ATTACHMENT", "SAMPLED"})}, {"clear_color", numbers({0, 0, 0, 1})}}));
    a.push(jobj({{"name", jstr("scene_depth")}, {"kind", jstr("DEPTH")}, {"format", jstr("D32_SFLOAT")},
                 {"sizing", jstr("SWAPCHAIN_RELATIVE")}, {"scale", jnum(1.0)},
                 {"usage", usage({"DEPTH_STENCIL_ATTACHMENT"})}, {"clear_depth", jnum(1.0)}, {"clear_stencil", jnum(0)}}));
    a.push(jobj({{"name", jstr("swapchain")}, {"kind", jstr("SWAPCHAIN_COLOR")}, {"format", jstr("B8G8R8A8_SRGB")},
                 {"sizing", jstr("SWAPCHAIN_RELATIVE")}, {"scale", jnum(1.0)}, {"usage", usage({"COLOR_ATTACHMENT"})},
                 {"clear_color", numbers({0, 0, 0, 1})}}));
    return a;
}

JsonValue normalize_profile(const JsonValue& raw_in) {
    const JsonValue& r = obj_or_empty(&raw_in);
    JsonValue p = normalize_section(r, profile_scalars());
    p.set("version", jnum(kProfileSchemaVersion));

    JsonValue attachments = JsonValue::make_array();
    if (const JsonValue* a = r.find("attachments"); a && a->is_array()) {
        for (const auto& x : *a->a) attachments.push(normalize_attachment(obj_or_empty(&x)));
    }
    p.set("attachments", attachments.a->empty() ? default_attachments() : attachments);

    JsonValue passes = JsonValue::make_array();
    if (const JsonValue* a = r.find("render_passes"); a && a->is_array()) {
        for (const auto& x : *a->a) passes.push(normalize_render_pass(obj_or_empty(&x)));
    }
    p.set("render_passes", passes);

    JsonValue post = JsonValue::make_array();
    if (const JsonValue* a = r.find("post_process"); a && a->is_array()) {
        for (const auto& x : *a->a) post.push(normalize_post_process(obj_or_empty(&x)));
    }
    p.set("post_process", post);

    for (const auto& t : tail_sections()) p.set(t.key, normalize_section(obj_or_empty(r.find(t.key)), *t.section));
    if (auto ed = normalize_editor(r.find("_editor"))) p.set("_editor", *ed);
    return p;
}

std::string serialize_profile(const JsonValue& n) {
    OrderedJson o = OrderedJson::object();
    o.add("version", OrderedJson::from(jnum(kProfileSchemaVersion)));
    for (const Field& f : profile_scalars().fields) o.add(f.key, OrderedJson::from(*n.find(f.key)));

    OrderedJson attachments = OrderedJson::array();
    for (const auto& a : *n.find("attachments")->a) attachments.push(emit_attachment(a));
    o.add("attachments", std::move(attachments));
    OrderedJson passes = OrderedJson::array();
    for (const auto& p : *n.find("render_passes")->a) passes.push(emit_render_pass(p));
    o.add("render_passes", std::move(passes));
    OrderedJson post = OrderedJson::array();
    for (const auto& p : *n.find("post_process")->a) post.push(emit_post_process(p));
    o.add("post_process", std::move(post));

    for (const auto& t : tail_sections()) o.add(t.key, emit_section(*n.find(t.key), *t.section));
    if (const JsonValue* ed = n.find("_editor")) o.add("_editor", OrderedJson::from(*ed));
    return o.pretty() + "\n";
}

std::string profile_file_name(const std::string& profile_name) {
    std::string slug;
    for (char c : profile_name) {
        char l = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        if ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9') || l == '_' || l == '-') slug.push_back(l);
    }
    return (slug.empty() ? std::string("untitled") : slug) + ".profile.json";
}

} // namespace ergo::studio::plugins::render_pipeline
