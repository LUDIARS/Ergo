#include "ergo/ui_kit/ui_kit.h"

#include "ergo/common/json_min.h"
#include "ergo/io/file.h"

#include <algorithm>
#include <cstdio>

namespace ergo::ui_kit {

namespace jm = ergo::common::jsonm;

// ===========================================================================
// JSON parse / serialize
// ===========================================================================
namespace {

NodeKind  node_kind_from(const std::string& s) {
    if (s == "text")      return NodeKind::Text;
    if (s == "image")     return NodeKind::Image;
    if (s == "shape")     return NodeKind::Shape;
    if (s == "nineslice") return NodeKind::NineSlice;
    return NodeKind::Frame;
}
const char* node_kind_str(NodeKind k) {
    switch (k) {
        case NodeKind::Text:      return "text";
        case NodeKind::Image:     return "image";
        case NodeKind::Shape:     return "shape";
        case NodeKind::NineSlice: return "nineslice";
        default:                  return "frame";
    }
}
LayoutMode layout_mode_from(const std::string& s) {
    if (s == "horizontal") return LayoutMode::Horizontal;
    if (s == "vertical")   return LayoutMode::Vertical;
    return LayoutMode::None;
}
const char* layout_mode_str(LayoutMode m) {
    switch (m) {
        case LayoutMode::Horizontal: return "horizontal";
        case LayoutMode::Vertical:   return "vertical";
        default:                     return "none";
    }
}
Sizing sizing_from(const std::string& s) {
    if (s == "hug")  return Sizing::Hug;
    if (s == "fill") return Sizing::Fill;
    return Sizing::Fixed;
}
const char* sizing_str(Sizing s) {
    switch (s) { case Sizing::Hug: return "hug"; case Sizing::Fill: return "fill";
                 default: return "fixed"; }
}
AlignMain align_main_from(const std::string& s) {
    if (s == "center")        return AlignMain::Center;
    if (s == "end")           return AlignMain::End;
    if (s == "space_between") return AlignMain::SpaceBetween;
    return AlignMain::Start;
}
const char* align_main_str(AlignMain a) {
    switch (a) { case AlignMain::Center: return "center"; case AlignMain::End: return "end";
                 case AlignMain::SpaceBetween: return "space_between"; default: return "start"; }
}
AlignCross align_cross_from(const std::string& s) {
    if (s == "center")  return AlignCross::Center;
    if (s == "end")     return AlignCross::End;
    if (s == "stretch") return AlignCross::Stretch;
    return AlignCross::Start;
}
const char* align_cross_str(AlignCross a) {
    switch (a) { case AlignCross::Center: return "center"; case AlignCross::End: return "end";
                 case AlignCross::Stretch: return "stretch"; default: return "start"; }
}
Pin pin_from(const std::string& s) {
    if (s == "center")  return Pin::Center;
    if (s == "end")     return Pin::End;
    if (s == "stretch") return Pin::Stretch;
    if (s == "scale")   return Pin::Scale;
    return Pin::Start;
}
const char* pin_str(Pin p) {
    switch (p) { case Pin::Center: return "center"; case Pin::End: return "end";
                 case Pin::Stretch: return "stretch"; case Pin::Scale: return "scale";
                 default: return "start"; }
}
TextAlign text_align_from(const std::string& s) {
    if (s == "center") return TextAlign::Center;
    if (s == "right")  return TextAlign::Right;
    return TextAlign::Left;
}
const char* text_align_str(TextAlign a) {
    switch (a) { case TextAlign::Center: return "center"; case TextAlign::Right: return "right";
                 default: return "left"; }
}

float jnum(const jm::JsonValue* o, const char* k, float fb) {
    if (!o) return fb;
    const jm::JsonValue* v = o->find(k);
    return v ? static_cast<float>(v->as_number(fb)) : fb;
}
bool jbool(const jm::JsonValue* o, const char* k, bool fb) {
    if (!o) return fb;
    const jm::JsonValue* v = o->find(k);
    return v ? v->as_bool(fb) : fb;
}
std::string jstr(const jm::JsonValue* o, const char* k, const std::string& fb) {
    if (!o) return fb;
    const jm::JsonValue* v = o->find(k);
    return (v && v->is_string()) ? v->s : fb;
}
Color jcolor(const jm::JsonValue* o, const char* k, Color fb) {
    if (!o) return fb;
    const jm::JsonValue* v = o->find(k);
    if (!v || !v->is_array() || !v->a || v->a->size() < 3) return fb;
    const auto& a = *v->a;
    Color c;
    c.r = static_cast<float>(a[0].as_number(fb.r));
    c.g = static_cast<float>(a[1].as_number(fb.g));
    c.b = static_cast<float>(a[2].as_number(fb.b));
    c.a = a.size() >= 4 ? static_cast<float>(a[3].as_number(1.0)) : 1.0f;
    return c;
}

void parse_node(const jm::JsonValue& j, Node& n) {
    if (!j.is_object()) return;
    n.name    = jstr(&j, "name", "");
    n.kind    = node_kind_from(jstr(&j, "kind", "frame"));
    n.position = {jnum(&j, "x", 0.0f), jnum(&j, "y", 0.0f)};
    n.size     = {jnum(&j, "w", 0.0f), jnum(&j, "h", 0.0f)};
    n.opacity  = jnum(&j, "opacity", 1.0f);
    n.visible  = jbool(&j, "visible", true);
    n.clip     = jbool(&j, "clip", false);
    n.text     = jstr(&j, "text", "");

    if (const jm::JsonValue* l = j.find("layout")) {
        LayoutSpec& ls = n.layout;
        ls.mode        = layout_mode_from(jstr(l, "mode", "none"));
        ls.gap         = jnum(l, "gap", 0.0f);
        ls.align_main  = align_main_from(jstr(l, "align_main", "start"));
        ls.align_cross = align_cross_from(jstr(l, "align_cross", "start"));
        ls.w_sizing    = sizing_from(jstr(l, "w_sizing", "fixed"));
        ls.h_sizing    = sizing_from(jstr(l, "h_sizing", "fixed"));
        ls.pin_x       = pin_from(jstr(l, "pin_x", "start"));
        ls.pin_y       = pin_from(jstr(l, "pin_y", "start"));
        if (const jm::JsonValue* p = l->find("pad")) {
            if (p->is_array() && p->a && p->a->size() >= 4) {
                ls.pad_l = static_cast<float>((*p->a)[0].as_number(0));
                ls.pad_t = static_cast<float>((*p->a)[1].as_number(0));
                ls.pad_r = static_cast<float>((*p->a)[2].as_number(0));
                ls.pad_b = static_cast<float>((*p->a)[3].as_number(0));
            }
        }
    }
    if (const jm::JsonValue* s = j.find("style")) {
        Style& st = n.style;
        st.fill          = jcolor(s, "fill",   {0, 0, 0, 0});
        st.stroke        = jcolor(s, "stroke", {0, 0, 0, 0});
        st.stroke_width  = jnum(s, "stroke_width", 0.0f);
        st.corner_radius = jnum(s, "corner_radius", 0.0f);
        st.text_color    = jcolor(s, "text_color", {1, 1, 1, 1});
        st.font_size     = jnum(s, "font_size", 16.0f);
        st.text_align    = text_align_from(jstr(s, "text_align", "left"));
        st.texture_id    = static_cast<uint32_t>(jnum(s, "texture_id", 0.0f));
        if (const jm::JsonValue* nn = s->find("nine")) {
            if (nn->is_array() && nn->a && nn->a->size() >= 4) {
                st.nine_l = static_cast<float>((*nn->a)[0].as_number(0));
                st.nine_t = static_cast<float>((*nn->a)[1].as_number(0));
                st.nine_r = static_cast<float>((*nn->a)[2].as_number(0));
                st.nine_b = static_cast<float>((*nn->a)[3].as_number(0));
            }
        }
    }
    if (const jm::JsonValue* ch = j.find("children")) {
        if (ch->is_array() && ch->a) {
            n.children.reserve(ch->a->size());
            for (const auto& cj : *ch->a) {
                Node cn;
                parse_node(cj, cn);
                n.children.push_back(std::move(cn));
            }
        }
    }
}

jm::JsonValue color_json(const Color& c) {
    jm::JsonValue a = jm::JsonValue::make_array();
    a.push(jm::JsonValue::make_number(c.r));
    a.push(jm::JsonValue::make_number(c.g));
    a.push(jm::JsonValue::make_number(c.b));
    a.push(jm::JsonValue::make_number(c.a));
    return a;
}

jm::JsonValue node_json(const Node& n) {
    jm::JsonValue o = jm::JsonValue::make_object();
    o.set("name",    jm::JsonValue::make_string(n.name));
    o.set("kind",    jm::JsonValue::make_string(node_kind_str(n.kind)));
    o.set("x",       jm::JsonValue::make_number(n.position.x));
    o.set("y",       jm::JsonValue::make_number(n.position.y));
    o.set("w",       jm::JsonValue::make_number(n.size.x));
    o.set("h",       jm::JsonValue::make_number(n.size.y));
    o.set("opacity", jm::JsonValue::make_number(n.opacity));
    o.set("visible", jm::JsonValue::make_bool(n.visible));
    o.set("clip",    jm::JsonValue::make_bool(n.clip));
    if (!n.text.empty()) o.set("text", jm::JsonValue::make_string(n.text));

    jm::JsonValue l = jm::JsonValue::make_object();
    l.set("mode",        jm::JsonValue::make_string(layout_mode_str(n.layout.mode)));
    l.set("gap",         jm::JsonValue::make_number(n.layout.gap));
    l.set("align_main",  jm::JsonValue::make_string(align_main_str(n.layout.align_main)));
    l.set("align_cross", jm::JsonValue::make_string(align_cross_str(n.layout.align_cross)));
    l.set("w_sizing",    jm::JsonValue::make_string(sizing_str(n.layout.w_sizing)));
    l.set("h_sizing",    jm::JsonValue::make_string(sizing_str(n.layout.h_sizing)));
    l.set("pin_x",       jm::JsonValue::make_string(pin_str(n.layout.pin_x)));
    l.set("pin_y",       jm::JsonValue::make_string(pin_str(n.layout.pin_y)));
    jm::JsonValue pad = jm::JsonValue::make_array();
    pad.push(jm::JsonValue::make_number(n.layout.pad_l));
    pad.push(jm::JsonValue::make_number(n.layout.pad_t));
    pad.push(jm::JsonValue::make_number(n.layout.pad_r));
    pad.push(jm::JsonValue::make_number(n.layout.pad_b));
    l.set("pad", std::move(pad));
    o.set("layout", std::move(l));

    jm::JsonValue s = jm::JsonValue::make_object();
    s.set("fill",          color_json(n.style.fill));
    s.set("stroke",        color_json(n.style.stroke));
    s.set("stroke_width",  jm::JsonValue::make_number(n.style.stroke_width));
    s.set("corner_radius", jm::JsonValue::make_number(n.style.corner_radius));
    s.set("text_color",    color_json(n.style.text_color));
    s.set("font_size",     jm::JsonValue::make_number(n.style.font_size));
    s.set("text_align",    jm::JsonValue::make_string(text_align_str(n.style.text_align)));
    s.set("texture_id",    jm::JsonValue::make_number(n.style.texture_id));
    jm::JsonValue nine = jm::JsonValue::make_array();
    nine.push(jm::JsonValue::make_number(n.style.nine_l));
    nine.push(jm::JsonValue::make_number(n.style.nine_t));
    nine.push(jm::JsonValue::make_number(n.style.nine_r));
    nine.push(jm::JsonValue::make_number(n.style.nine_b));
    s.set("nine", std::move(nine));
    o.set("style", std::move(s));

    jm::JsonValue ch = jm::JsonValue::make_array();
    for (const auto& c : n.children) ch.push(node_json(c));
    o.set("children", std::move(ch));
    return o;
}

} // namespace

bool parse_document(const std::string& json_text, Document& out) {
    jm::JsonValue root;
    if (!jm::parse(json_text, root) || !root.is_object()) return false;
    const jm::JsonValue* r = root.find("root");
    if (!r) return false;
    out.root = Node{};
    parse_node(*r, out.root);
    return true;
}

bool load_document(const std::string& path, Document& out) {
    std::string text;
    if (!ergo::io::read_file(path, text)) {
        std::fprintf(stderr, "[ui_kit] uidoc 読込失敗: %s\n", path.c_str());
        return false;
    }
    if (!parse_document(text, out)) {
        std::fprintf(stderr, "[ui_kit] uidoc パース失敗: %s\n", path.c_str());
        return false;
    }
    return true;
}

std::string serialize_document(const Document& doc) {
    jm::JsonValue root = jm::JsonValue::make_object();
    root.set("root", node_json(doc.root));
    return jm::serialize(root);
}

// ===========================================================================
// UIContext — layout solve + draw list
// ===========================================================================

void UIContext::set_document(Document doc) { doc_ = std::move(doc); }

void UIContext::set_viewport(int width, int height) {
    vw_ = (width  > 0) ? width  : 1;
    vh_ = (height > 0) ? height : 1;
}

// 内在サイズの測定 (post-order)。 Hug ノードは子から積み上げる。
// 結果は n.resolved_rect.w / .h に一時的に書く (place が最終 rect で上書き)。
void UIContext::measure_(Node& n) {
    for (auto& c : n.children) measure_(c);

    float w = n.size.x;
    float h = n.size.y;

    if (n.layout.mode != LayoutMode::None) {
        const bool horiz = (n.layout.mode == LayoutMode::Horizontal);
        float main_content = 0.0f, cross_content = 0.0f;
        int   visible_count = 0;
        for (const auto& c : n.children) {
            if (!c.visible) continue;
            const float cw = c.resolved_rect.w;  // 子の測定済みサイズ
            const float ch = c.resolved_rect.h;
            main_content  += horiz ? cw : ch;
            cross_content  = std::max(cross_content, horiz ? ch : cw);
            ++visible_count;
        }
        if (visible_count > 1) main_content += n.layout.gap * (visible_count - 1);
        const float pad_main  = horiz ? (n.layout.pad_l + n.layout.pad_r)
                                      : (n.layout.pad_t + n.layout.pad_b);
        const float pad_cross = horiz ? (n.layout.pad_t + n.layout.pad_b)
                                      : (n.layout.pad_l + n.layout.pad_r);
        const float hug_main  = main_content  + pad_main;
        const float hug_cross = cross_content + pad_cross;
        if (horiz) {
            if (n.layout.w_sizing == Sizing::Hug) w = hug_main;
            if (n.layout.h_sizing == Sizing::Hug) h = hug_cross;
        } else {
            if (n.layout.h_sizing == Sizing::Hug) h = hug_main;
            if (n.layout.w_sizing == Sizing::Hug) w = hug_cross;
        }
    }
    n.resolved_rect.w = w;
    n.resolved_rect.h = h;
}

// 最終配置 (pre-order)。 frame = この node が占有する画面矩形。
void UIContext::solve_layout_(Node& n, const Rect& frame,
                              const std::string& parent_path) {
    n.resolved_rect = frame;
    n.resolved_path = parent_path.empty() ? n.name
                                          : (parent_path + "/" + n.name);

    if (n.children.empty()) return;

    if (n.layout.mode == LayoutMode::None) {
        // constraints: 各子を position/size + pin で配置する。
        for (auto& c : n.children) {
            Rect r;
            const float cw = c.resolved_rect.w;
            const float ch = c.resolved_rect.h;
            // X
            switch (c.layout.pin_x) {
                case Pin::Center:
                    r.x = frame.x + (frame.w - cw) * 0.5f + c.position.x; r.w = cw; break;
                case Pin::End:
                    r.x = frame.x + frame.w - cw - c.position.x; r.w = cw; break;
                case Pin::Stretch:
                    r.x = frame.x + c.position.x;
                    r.w = frame.w - c.position.x * 2.0f; break;
                default:  // Start / Scale (P1 簡略)
                    r.x = frame.x + c.position.x; r.w = cw; break;
            }
            // Y
            switch (c.layout.pin_y) {
                case Pin::Center:
                    r.y = frame.y + (frame.h - ch) * 0.5f + c.position.y; r.h = ch; break;
                case Pin::End:
                    r.y = frame.y + frame.h - ch - c.position.y; r.h = ch; break;
                case Pin::Stretch:
                    r.y = frame.y + c.position.y;
                    r.h = frame.h - c.position.y * 2.0f; break;
                default:
                    r.y = frame.y + c.position.y; r.h = ch; break;
            }
            solve_layout_(c, r, n.resolved_path);
        }
        return;
    }

    // auto-layout: 主軸に子を詰める。
    const bool horiz = (n.layout.mode == LayoutMode::Horizontal);
    const Rect inner{frame.x + n.layout.pad_l, frame.y + n.layout.pad_t,
                     frame.w - n.layout.pad_l - n.layout.pad_r,
                     frame.h - n.layout.pad_t - n.layout.pad_b};
    const float avail_main  = horiz ? inner.w : inner.h;
    const float avail_cross = horiz ? inner.h : inner.w;

    // Fill 子へ配る余白を計算する。
    float fixed_main = 0.0f;
    int   visible_count = 0, fill_count = 0;
    for (const auto& c : n.children) {
        if (!c.visible) continue;
        ++visible_count;
        const Sizing main_sz = horiz ? c.layout.w_sizing : c.layout.h_sizing;
        if (main_sz == Sizing::Fill) ++fill_count;
        else fixed_main += horiz ? c.resolved_rect.w : c.resolved_rect.h;
    }
    const float total_gap = (visible_count > 1) ? n.layout.gap * (visible_count - 1) : 0.0f;
    float leftover  = avail_main - fixed_main - total_gap;
    const float fill_share = (fill_count > 0 && leftover > 0.0f)
                                 ? leftover / static_cast<float>(fill_count) : 0.0f;

    // 主軸の開始オフセット (align_main)。
    float cursor = 0.0f;
    float extra_gap = n.layout.gap;
    if (fill_count == 0) {
        const float used = fixed_main + total_gap;
        switch (n.layout.align_main) {
            case AlignMain::Center: cursor = (avail_main - used) * 0.5f; break;
            case AlignMain::End:    cursor = (avail_main - used);        break;
            case AlignMain::SpaceBetween:
                if (visible_count > 1)
                    extra_gap = n.layout.gap +
                                std::max(0.0f, avail_main - used) / (visible_count - 1);
                break;
            default: break;  // Start
        }
    }

    for (auto& c : n.children) {
        if (!c.visible) { c.resolved_rect = {}; continue; }
        const Sizing main_sz  = horiz ? c.layout.w_sizing : c.layout.h_sizing;
        const Sizing cross_sz = horiz ? c.layout.h_sizing : c.layout.w_sizing;
        const float c_main  = (main_sz == Sizing::Fill)
                                  ? fill_share
                                  : (horiz ? c.resolved_rect.w : c.resolved_rect.h);
        float c_cross = (horiz ? c.resolved_rect.h : c.resolved_rect.w);
        if (cross_sz == Sizing::Fill || n.layout.align_cross == AlignCross::Stretch)
            c_cross = avail_cross;

        // 交差軸オフセット (align_cross)。
        float cross_off = 0.0f;
        switch (n.layout.align_cross) {
            case AlignCross::Center: cross_off = (avail_cross - c_cross) * 0.5f; break;
            case AlignCross::End:    cross_off = (avail_cross - c_cross);        break;
            default: break;  // Start / Stretch
        }

        Rect r;
        if (horiz) {
            r = {inner.x + cursor, inner.y + cross_off, c_main, c_cross};
        } else {
            r = {inner.x + cross_off, inner.y + cursor, c_cross, c_main};
        }
        solve_layout_(c, r, n.resolved_path);
        cursor += c_main + extra_gap;
    }
}

void UIContext::emit_(const Node& n, float inherited_opacity) {
    if (!n.visible) return;
    const float op = inherited_opacity * n.opacity;
    const Rect& r  = n.resolved_rect;

    // 背景塗り / 種別ごとの描画。
    if (n.kind == NodeKind::NineSlice && n.style.texture_id != 0) {
        DrawCmd d;
        d.kind = DrawKind::NineSlice;
        d.rect = r;
        d.color = n.style.fill;
        d.texture_id = n.style.texture_id;
        d.nine[0] = n.style.nine_l; d.nine[1] = n.style.nine_t;
        d.nine[2] = n.style.nine_r; d.nine[3] = n.style.nine_b;
        d.opacity = op;
        draw_list_.push_back(std::move(d));
    } else if (n.kind == NodeKind::Image && n.style.texture_id != 0) {
        DrawCmd d;
        d.kind = DrawKind::Image;
        d.rect = r;
        d.color = n.style.fill.a > 0.0f ? n.style.fill : Color{1, 1, 1, 1};  // tint
        d.texture_id = n.style.texture_id;
        d.opacity = op;
        draw_list_.push_back(std::move(d));
    } else if (n.style.fill.a > 0.0f) {
        DrawCmd d;
        d.kind = DrawKind::Rect;
        d.rect = r;
        d.color = n.style.fill;
        d.corner_radius = n.style.corner_radius;
        d.opacity = op;
        draw_list_.push_back(std::move(d));
    }

    // Text。
    if (n.kind == NodeKind::Text && !n.text.empty()) {
        DrawCmd d;
        d.kind = DrawKind::Text;
        d.rect = r;
        d.color = n.style.text_color;
        d.text = n.text;
        d.font_size = n.style.font_size;
        d.text_align = n.style.text_align;
        d.opacity = op;
        draw_list_.push_back(std::move(d));
    }

    // 子。 clip フレームは PushClip/PopClip で挟む。
    const bool clip = n.clip && !n.children.empty();
    if (clip) {
        DrawCmd c;
        c.kind = DrawKind::PushClip;
        c.rect = r;
        draw_list_.push_back(std::move(c));
    }
    for (const auto& ch : n.children) emit_(ch, op);
    if (clip) {
        DrawCmd c;
        c.kind = DrawKind::PopClip;
        draw_list_.push_back(std::move(c));
    }
}

void UIContext::update(float dt) {
    // アニメ進行 (プロパティを動かす) → レイアウト解決 → 入力ディスパッチ →
    // 描画リスト生成。
    advance_anims_(dt);
    measure_(doc_.root);
    Rect root_frame{0.0f, 0.0f,
                    doc_.root.size.x > 0.0f ? doc_.root.size.x : static_cast<float>(vw_),
                    doc_.root.size.y > 0.0f ? doc_.root.size.y : static_cast<float>(vh_)};
    solve_layout_(doc_.root, root_frame, "");
    dispatch_input_();
    draw_list_.clear();
    emit_(doc_.root, 1.0f);
}

// ── アニメーション ─────────────────────────────────────────────────────
namespace {
float ease_apply(Easing e, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    switch (e) {
        case Easing::EaseIn:    return t * t;
        case Easing::EaseOut:   return 1.0f - (1.0f - t) * (1.0f - t);
        case Easing::EaseInOut: return t < 0.5f ? 2.0f * t * t
                                                : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
        default:                return t;  // Linear
    }
}
float* prop_ptr(Node& n, AnimProp p) {
    switch (p) {
        case AnimProp::Opacity: return &n.opacity;
        case AnimProp::PosX:    return &n.position.x;
        case AnimProp::PosY:    return &n.position.y;
        case AnimProp::SizeW:   return &n.size.x;
        case AnimProp::SizeH:   return &n.size.y;
        case AnimProp::FillR:   return &n.style.fill.r;
        case AnimProp::FillG:   return &n.style.fill.g;
        case AnimProp::FillB:   return &n.style.fill.b;
        case AnimProp::FillA:   return &n.style.fill.a;
        default:                return nullptr;
    }
}
} // namespace

void UIContext::animate(const std::string& node_path, AnimProp prop, float to,
                        float duration, Easing easing) {
    // 同じ (path, prop) の既存トラックは置き換える。
    for (auto& a : anims_)
        if (a.path == node_path && a.prop == prop) {
            a.to = to; a.dur = (duration > 1e-4f) ? duration : 1e-4f;
            a.t = 0.0f; a.easing = easing; a.started = false;
            return;
        }
    AnimTrack tr;
    tr.path = node_path; tr.prop = prop; tr.to = to;
    tr.dur = (duration > 1e-4f) ? duration : 1e-4f;
    tr.easing = easing;
    anims_.push_back(std::move(tr));
}

void UIContext::stop_anim(const std::string& node_path) {
    anims_.erase(std::remove_if(anims_.begin(), anims_.end(),
        [&](const AnimTrack& a) {
            return a.path == node_path ||
                   (a.path.size() > node_path.size() &&
                    a.path.compare(0, node_path.size(), node_path) == 0 &&
                    a.path[node_path.size()] == '/');
        }), anims_.end());
}

void UIContext::advance_anims_(float dt) {
    for (std::size_t i = 0; i < anims_.size();) {
        AnimTrack& a = anims_[i];
        Node* n = find(a.path);
        if (!n) { anims_[i] = anims_.back(); anims_.pop_back(); continue; }
        float* dst = prop_ptr(*n, a.prop);
        if (!dst) { anims_[i] = anims_.back(); anims_.pop_back(); continue; }
        if (!a.started) { a.from = *dst; a.started = true; }
        a.t += dt / a.dur;
        const float k = ease_apply(a.easing, a.t);
        *dst = a.from + (a.to - a.from) * k;
        if (a.t >= 1.0f) {
            *dst = a.to;
            anims_[i] = anims_.back();
            anims_.pop_back();
            continue;
        }
        ++i;
    }
}

// ── 入力ディスパッチ ───────────────────────────────────────────────────
void UIContext::feed_input(const PointerInput& p) { pointer_ = p; }

void UIContext::on(const std::string& node_path, EventKind kind, EventHandler cb) {
    handlers_[node_path].push_back({kind, std::move(cb)});
}

namespace {
bool rect_contains(const Rect& r, float x, float y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}
// 前面 (描画順で後) を優先して、 点を含む最前面ノードのパスを返す。
const Node* hit_test(const Node& n, float x, float y) {
    if (!n.visible) return nullptr;
    if (!rect_contains(n.resolved_rect, x, y)) {
        // clip フレームは範囲外なら子も無視。 非 clip でも自分が外なら
        // 子が外にはみ出ない前提で打ち切る (UI は通常そうなる)。
        return nullptr;
    }
    // 子を逆順 (後の子 = 前面) に走査。
    for (auto it = n.children.rbegin(); it != n.children.rend(); ++it) {
        const Node* h = hit_test(*it, x, y);
        if (h) return h;
    }
    return &n;
}
} // namespace

void UIContext::dispatch_event_(Event& ev) {
    // 起点パス → 祖先へバブリング。 パスを末尾から切り詰めて辿る。
    std::string path = ev.node_path;
    while (true) {
        auto it = handlers_.find(path);
        if (it != handlers_.end()) {
            for (auto& h : it->second)
                if (h.kind == ev.kind) {
                    h.cb(ev);
                    if (!ev.propagate) return;
                }
        }
        const std::size_t slash = path.rfind('/');
        if (slash == std::string::npos) break;
        path.resize(slash);
    }
}

void UIContext::dispatch_input_() {
    const Node* hit = hit_test(doc_.root, pointer_.x, pointer_.y);
    const std::string hit_path = hit ? hit->resolved_path : std::string{};

    if (first_input_) {
        prev_pointer_ = pointer_;
        hovered_path_ = hit_path;
        first_input_ = false;
        return;
    }

    // hover enter / exit
    if (hit_path != hovered_path_) {
        if (!hovered_path_.empty()) {
            Event e{EventKind::PointerExit, hovered_path_, pointer_.x, pointer_.y, true};
            dispatch_event_(e);
        }
        if (!hit_path.empty()) {
            Event e{EventKind::PointerEnter, hit_path, pointer_.x, pointer_.y, true};
            dispatch_event_(e);
        }
        hovered_path_ = hit_path;
    }

    // press / release / click
    if (pointer_.down && !prev_pointer_.down) {
        pressed_path_ = hit_path;
        if (!hit_path.empty()) {
            Event e{EventKind::PointerDown, hit_path, pointer_.x, pointer_.y, true};
            dispatch_event_(e);
        }
    } else if (!pointer_.down && prev_pointer_.down) {
        if (!hit_path.empty()) {
            Event e{EventKind::PointerUp, hit_path, pointer_.x, pointer_.y, true};
            dispatch_event_(e);
        }
        if (!pressed_path_.empty() && pressed_path_ == hit_path) {
            Event e{EventKind::Click, hit_path, pointer_.x, pointer_.y, true};
            dispatch_event_(e);
        }
        pressed_path_.clear();
    }

    prev_pointer_ = pointer_;
}

Node* UIContext::find(const std::string& path) {
    Node* cur = &doc_.root;
    std::size_t start = 0;
    // 先頭セグメントは root の名前に一致すること (一致しなければ root から下る)。
    while (start <= path.size()) {
        std::size_t slash = path.find('/', start);
        std::string seg = (slash == std::string::npos)
                              ? path.substr(start)
                              : path.substr(start, slash - start);
        if (start == 0 && seg == cur->name) {
            // root セグメント — 何もしない
        } else if (!seg.empty()) {
            Node* next = nullptr;
            for (auto& c : cur->children) {
                if (c.name == seg) { next = &c; break; }
            }
            if (!next) return nullptr;
            cur = next;
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return cur;
}

} // namespace ergo::ui_kit
