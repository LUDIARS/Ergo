#include "ergo/studio/ui/ui_kit_emitter.h"

namespace ergo::studio::ui {

using ui_kit::Color;
using ui_kit::LayoutMode;
using ui_kit::Node;
using ui_kit::NodeKind;
using ui_kit::Sizing;

namespace {

class Emitter {
public:
    Emitter(const ITextMeasure& m, const Theme& t, Bindings& b) : m_(m), t_(t), b_(b) {}

    void children(const ViewNode& tree, Node& out, const std::string& path) {
        for (const ViewNode& c : tree.children) out.children.push_back(node(c, path + "/" + c.name));
    }

    /// Container settings a view set on its builder's node.
    void container(const ViewNode& v, Node& n) const {
        if (v.width > 0) {
            n.layout.w_sizing = Sizing::Fixed;
            n.size.x          = v.width;
        }
        if (v.align_start) n.layout.align_cross = ui_kit::AlignCross::Start;
        if (v.has_fill) n.style.fill = v.fill;
    }

private:
    Node text(std::string name, const std::string& s, float size, const Color& color) const {
        Node t;
        t.name             = std::move(name);
        t.kind             = NodeKind::Text;
        t.text             = s;
        t.style.font_size  = size;
        t.style.text_color = color;
        t.size             = {m_.width(s, size), m_.line_height(size)};
        return t;
    }

    Node node(const ViewNode& v, const std::string& path) {
        switch (v.kind) {
            case ViewKind::Text: return text(v.name, v.text, v.font_size, v.color);
            case ViewKind::Button: return button(v, path);
            case ViewKind::Field: return field(v, path);
            case ViewKind::ListItem: return list_item(v, path);
            case ViewKind::Slider: return slider(v, path);
            case ViewKind::Status: return status(v);
            case ViewKind::Separator: return separator(v);
            case ViewKind::Spacer: {
                Node s;
                s.name = v.name;
                s.size = {1, v.height};
                return s;
            }
            case ViewKind::Video: return video(v);
            case ViewKind::Row: return row(v, path);
            case ViewKind::Panel: return panel(v, path);
            case ViewKind::Group: break;
        }
        Node g;
        g.name        = v.name;
        g.layout.mode = LayoutMode::Vertical;
        container(v, g);
        children(v, g, path);
        return g;
    }

    Node button(const ViewNode& v, const std::string& path) {
        Node b;
        b.name                = v.name;
        b.kind                = NodeKind::Frame;
        b.style.fill          = v.primary ? t_.accent : t_.panel_alt;
        b.style.corner_radius = 4;
        b.layout.mode         = LayoutMode::Horizontal;
        b.layout.pad_l = b.layout.pad_r = 12;
        b.layout.pad_t = b.layout.pad_b = 6;
        b.layout.w_sizing = b.layout.h_sizing = Sizing::Hug;
        b.children.push_back(text("text", v.text, t_.font_size, t_.text));
        if (v.on_click) b_.clicks.push_back({path, v.on_click});
        return b;
    }

    Node field(const ViewNode& v, const std::string& path) {
        Node f;
        f.name        = v.name;
        f.kind        = NodeKind::Frame;
        f.layout.mode = LayoutMode::Vertical;
        f.layout.pad_l = f.layout.pad_r = 8;
        f.layout.pad_t = f.layout.pad_b = 5;
        f.clip                 = true;
        const bool focused     = b_.focused == path;
        f.style.fill           = focused ? t_.panel : t_.panel_alt;
        f.style.corner_radius  = 4;
        const float lh         = m_.line_height(t_.font_size);
        f.size                 = {v.width, lh * static_cast<float>(v.lines) + 10};
        const float inner      = v.width - 16;
        const std::string caret = focused ? "|" : "";
        const std::string value = v.value ? *v.value : std::string();
        if (value.empty() && !focused) {
            f.children.push_back(text("ph", fit_text(v.text, inner, t_.font_size, m_), t_.font_size, t_.muted));
        } else if (v.secret) {
            std::string dots;
            for (unsigned char c : value) {
                if ((c & 0xC0) != 0x80) dots += "\xE2\x80\xA2";   // one bullet per code point
            }
            f.children.push_back(text("text", fit_tail(dots + caret, inner, t_.font_size, m_), t_.font_size, t_.text));
        } else if (!v.multiline) {
            f.children.push_back(text("text", fit_tail(value + caret, inner, t_.font_size, m_), t_.font_size, t_.text));
        } else {
            // The last `lines` lines (the caret sits at the end of the text).
            std::vector<std::string> ls;
            std::size_t start = 0;
            const std::string all = value + caret;
            while (true) {
                const auto end = all.find('\n', start);
                ls.push_back(all.substr(start, end == std::string::npos ? std::string::npos : end - start));
                if (end == std::string::npos) break;
                start = end + 1;
            }
            const auto rows = static_cast<std::size_t>(v.lines);
            const std::size_t first = ls.size() > rows ? ls.size() - rows : 0;
            for (std::size_t i = first; i < ls.size(); ++i) {
                f.children.push_back(text("line" + std::to_string(i), fit_tail(ls[i], inner, t_.font_size, m_),
                                          t_.font_size, t_.text));
            }
        }
        b_.fields.push_back({path, v.value, v.on_submit, v.multiline});
        // Clicking a field focuses it (the app owns focus; this only names the path).
        Bindings* b = &b_;
        b_.clicks.push_back({path, [b, path] { b->focused = path; }});
        return f;
    }

    Node list_item(const ViewNode& v, const std::string& path) {
        Node it;
        it.name               = v.name;
        it.kind               = NodeKind::Frame;
        it.layout.mode        = LayoutMode::Horizontal;
        it.layout.gap         = 10;
        it.layout.align_cross = ui_kit::AlignCross::Center;
        it.layout.pad_l = it.layout.pad_r = 8;
        it.layout.pad_t = it.layout.pad_b = 4;
        it.layout.w_sizing     = Sizing::Fill;
        it.layout.h_sizing     = Sizing::Hug;
        it.style.fill          = v.selected ? t_.accent : t_.panel_alt;
        it.style.corner_radius = 3;
        it.children.push_back(text("text", v.text, t_.font_size, t_.text));
        if (!v.detail.empty()) {
            it.children.push_back(text("detail", v.detail, t_.small_size, v.selected ? t_.text : t_.muted));
        }
        if (v.on_click) b_.clicks.push_back({path, v.on_click});
        return it;
    }

    Node slider(const ViewNode& v, const std::string& path) {
        Node track;
        track.name                = v.name;
        track.kind                = NodeKind::Frame;
        track.size                = {v.width, 14};
        track.layout.w_sizing     = Sizing::Fixed;
        track.layout.h_sizing     = Sizing::Fixed;
        track.style.fill          = t_.panel_alt;
        track.style.corner_radius = 3;
        Node fill;
        fill.name                = "fill";
        fill.kind                = NodeKind::Frame;
        fill.size                = {v.width * v.fraction, 14};
        fill.layout.w_sizing     = Sizing::Fixed;
        fill.layout.h_sizing     = Sizing::Fixed;
        fill.style.fill          = t_.accent;
        fill.style.corner_radius = 3;
        track.children.push_back(std::move(fill));
        if (v.on_set) b_.sliders.push_back({path, v.on_set});
        return track;
    }

    Node status(const ViewNode& v) const {
        Node r;
        r.name               = v.name;
        r.layout.mode        = LayoutMode::Horizontal;
        r.layout.gap         = 8;
        r.layout.align_cross = ui_kit::AlignCross::Center;
        r.layout.w_sizing = r.layout.h_sizing = Sizing::Hug;
        Node dot;
        dot.name                = "dot";
        dot.kind                = NodeKind::Shape;
        dot.size                = {10, 10};
        dot.style.fill          = v.ok ? t_.ok : t_.error;
        dot.style.corner_radius = 5;
        r.children.push_back(std::move(dot));
        r.children.push_back(text("label", v.text, t_.font_size, t_.text));
        return r;
    }

    Node separator(const ViewNode& v) const {
        Node s;
        s.name            = v.name;
        s.kind            = NodeKind::Shape;
        s.size            = {1, 1};
        s.style.fill      = t_.panel_alt;
        s.layout.w_sizing = Sizing::Fill;
        return s;
    }

    /// No media here: the frame and its label (the browser shows the stream).
    Node video(const ViewNode& v) const {
        Node f;
        f.name                = v.name;
        f.kind                = NodeKind::Frame;
        f.size                = {v.width, v.height};
        f.layout.w_sizing     = Sizing::Fixed;
        f.layout.h_sizing     = Sizing::Fixed;
        f.layout.mode         = LayoutMode::Vertical;
        f.layout.pad_l = f.layout.pad_t = 10;
        f.style.fill          = t_.background;
        f.style.corner_radius = 4;
        f.children.push_back(text("label", v.text, t_.small_size, t_.muted));
        return f;
    }

    Node row(const ViewNode& v, const std::string& path) {
        Node r;
        r.name               = v.name;
        r.layout.mode        = LayoutMode::Horizontal;
        r.layout.gap         = v.gap;
        r.layout.align_cross = ui_kit::AlignCross::Center;
        r.layout.w_sizing = r.layout.h_sizing = Sizing::Hug;
        container(v, r);
        children(v, r, path);
        return r;
    }

    Node panel(const ViewNode& v, const std::string& path) {
        Node p;
        p.name        = v.name;
        p.layout.mode = LayoutMode::Vertical;
        p.layout.gap  = v.gap;
        p.layout.pad_l = p.layout.pad_r = p.layout.pad_t = p.layout.pad_b = 10;
        p.layout.w_sizing     = Sizing::Fill;
        p.layout.h_sizing     = Sizing::Hug;
        p.style.fill          = t_.panel;
        p.style.corner_radius = 6;
        container(v, p);
        children(v, p, path);
        return p;
    }

    const ITextMeasure& m_;
    const Theme&        t_;
    Bindings&           b_;
};

} // namespace

void emit_ui_kit(const ViewNode& tree, ui_kit::Node& out, const std::string& out_path, const ITextMeasure& measure,
                 const Theme& theme, Bindings& bindings) {
    Emitter e(measure, theme, bindings);
    e.container(tree, out);
    e.children(tree, out, out_path);
}

} // namespace ergo::studio::ui
