#include "ergo/studio/tela/tela_emitter.h"

#include <tela/field_geometry.hpp>

#include <algorithm>
#include <cmath>

namespace ergo::studio::tela_shell {

using ui::ViewKind;
using ui::ViewNode;

namespace {

constexpr float kTextPad   = 2;
constexpr float kButtonPad = 6;
constexpr float kFieldPad  = 5;
constexpr float kPanelPad  = 10;

bool same(const ui_kit::Color& a, const ui_kit::Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

class Emitter {
public:
    Emitter(::tela::Document& doc, const EmitOptions& o) : doc_(doc), o_(o) {}

    void children(const ViewNode& parent, const std::string& prefix, bool in_row) {
        for (const ViewNode& c : parent.children) element(c, prefix + "/" + c.name, in_row);
    }

    float natural(const ViewNode& v) const {
        const auto& m = o_.measure;
        const auto& t = o_.theme;
        switch (v.kind) {
            case ViewKind::Text: return std::ceil(m.width(v.text, v.font_size)) + 2 * kTextPad + 2;
            case ViewKind::Button: return std::ceil(m.width(v.text, t.font_size)) + 2 * kButtonPad + 16;
            case ViewKind::Field:
            case ViewKind::Slider:
            case ViewKind::Video: return v.width;
            case ViewKind::ListItem:
                return std::ceil(m.width(v.text, t.font_size) + m.width(v.detail, t.font_size)) + 48;
            case ViewKind::Status: return std::ceil(m.width(v.text, t.font_size)) + 2 * kTextPad + 28;
            case ViewKind::Separator: return 8;
            case ViewKind::Spacer: return 1;
            case ViewKind::Row: {
                if (v.width > 0) return v.width;
                float w = 0;
                for (const auto& c : v.children) w += natural(c);
                if (v.children.size() > 1) w += v.gap * static_cast<float>(v.children.size() - 1);
                return w;
            }
            case ViewKind::Panel:
            case ViewKind::Group: {
                if (v.width > 0) return v.width;
                float w = 0;
                for (const auto& c : v.children) w = std::max(w, natural(c));
                return w + (v.kind == ViewKind::Panel ? 2 * kPanelPad : 0);
            }
        }
        return 0;
    }

private:
    std::function<void()> action(const ui::ClickFn& fn) const {
        if (!o_.interactive || !fn) return {};
        return [fn, touched = o_.touched] {
            fn();
            if (touched) touched();
        };
    }

    float text_scale(float font_size) const {
        return o_.theme.font_size > 0 && font_size > 0 ? font_size / o_.theme.font_size : 1.0f;
    }

    /// Width of an element: natural in a row, fixed when the view fixed it, else the column's.
    float width_of(const ViewNode& v, bool in_row) const {
        if (in_row) return natural(v);
        switch (v.kind) {
            case ViewKind::Button:
            case ViewKind::Field:
            case ViewKind::Slider:
            case ViewKind::Video: return natural(v);
            default: return v.width > 0 ? v.width : 0;
        }
    }

    void text(const ViewNode& v, const std::string& id, float w) {
        ::tela::Layout l;
        l.width      = w;
        l.padding    = kTextPad;
        l.text_scale = text_scale(v.font_size);
        const auto& t = o_.theme;
        if (same(v.color, t.error) || same(v.color, t.warn) || same(v.color, t.ok)) {
            const auto tone = same(v.color, t.error) ? ::tela::Tone::bad
                              : same(v.color, t.warn) ? ::tela::Tone::warning
                                                      : ::tela::Tone::good;
            doc_.status(id, v.text, tone, l);
            return;
        }
        doc_.text(id, v.text, l);
    }

    void field(const ViewNode& v, const std::string& id, float w) {
        ::tela::Layout l;
        l.width   = w;
        l.padding = kFieldPad;
        std::function<void(const std::string&)> change;
        if (o_.interactive && v.value) {
            change = [value = v.value, touched = o_.touched](const std::string& s) {
                *value = s;
                if (touched) touched();
            };
        }
        const std::string value = v.value ? *v.value : std::string();
        if (v.multiline) {
            l.lines = static_cast<unsigned>(std::max(1, v.lines));
            doc_.text_area(id, value, std::move(change), l);
            return;
        }
        ::tela::TextFieldOptions opt;
        opt.placeholder = v.text;
        opt.secret      = v.secret;
        opt.on_change   = std::move(change);
        opt.on_submit   = action(v.on_submit);
        doc_.text_field(id, value, std::move(opt), l);
    }

    void element(const ViewNode& v, const std::string& id, bool in_row) {
        const float w = width_of(v, in_row);
        switch (v.kind) {
            case ViewKind::Text: text(v, id, w); return;
            case ViewKind::Button: {
                ::tela::Layout l;
                l.width   = w;
                l.padding = kButtonPad;
                doc_.button(id, v.text, action(v.on_click), l);
                return;
            }
            case ViewKind::Field: field(v, id, w); return;
            case ViewKind::ListItem: {
                ::tela::Layout l;
                l.width   = w;
                l.padding = 4;
                doc_.list_item(id, v.text, v.detail, v.selected, action(v.on_click), l);
                return;
            }
            case ViewKind::Slider: {
                ::tela::Layout l;
                l.width   = w;
                l.padding = 4;
                std::function<void(float)> set;
                if (o_.interactive && v.on_set) {
                    set = [fn = v.on_set, touched = o_.touched](float f) {
                        fn(f);
                        if (touched) touched();
                    };
                }
                doc_.slider(id, v.fraction, std::move(set), l);
                return;
            }
            case ViewKind::Status: {
                ::tela::Layout l;
                l.width   = w;
                l.padding = kTextPad;
                doc_.status(id, v.text, v.ok ? ::tela::Tone::good : ::tela::Tone::bad, l);
                return;
            }
            case ViewKind::Separator: {
                ::tela::Layout l;
                l.width   = w;
                l.padding = 2;
                doc_.separator(id, l);
                return;
            }
            case ViewKind::Spacer: {
                ::tela::Layout l;
                l.width   = in_row ? 1 : 0;
                l.height  = std::max(1.0f, v.height);
                l.padding = 0;
                doc_.text(id, "", l);
                return;
            }
            case ViewKind::Video: {
                ::tela::Layout l;
                l.width   = w;
                l.height  = v.height;
                l.padding = 0;
                doc_.video(id, v.text, v.detail, l);
                return;
            }
            case ViewKind::Row:
            case ViewKind::Panel:
            case ViewKind::Group: {
                ::tela::Layout l;
                l.width = w;
                const bool row = v.kind == ViewKind::Row;
                l.flow    = row ? ::tela::Flow::row : ::tela::Flow::column;
                l.padding = v.kind == ViewKind::Panel ? kPanelPad : 0;
                l.gap     = v.gap;
                doc_.panel(id, [&] { children(v, id, row); }, l);
                return;
            }
        }
    }

    ::tela::Document&  doc_;
    const EmitOptions& o_;
};

} // namespace

void emit_children(const ViewNode& tree, ::tela::Document& doc, const std::string& prefix, const EmitOptions& o) {
    Emitter(doc, o).children(tree, prefix, tree.kind == ViewKind::Row);
}

float natural_width(const ViewNode& node, const EmitOptions& o) {
    ::tela::Document unused;
    return Emitter(unused, o).natural(node);
}

::tela::Color tela_color(const ui_kit::Color& c) {
    auto byte = [](float v) {
        const float x = v < 0 ? 0.0f : (v > 1 ? 1.0f : v);
        return static_cast<unsigned char>(std::lround(x * 255.0f));
    };
    return {byte(c.r), byte(c.g), byte(c.b), byte(c.a)};
}

::tela::Theme tela_theme(const ui::Theme& t) {
    ::tela::Theme out;
    out.panel       = tela_color(t.panel);
    out.text        = tela_color(t.text);
    out.button      = tela_color(t.panel_alt);
    out.pressed     = tela_color(t.accent);
    out.field       = tela_color(t.background);
    out.accent      = tela_color(t.accent);
    out.muted       = tela_color(t.muted);
    out.good        = tela_color(t.ok);
    out.warning     = tela_color(t.warn);
    out.bad         = tela_color(t.error);
    out.font_size   = t.font_size;
    out.line_height = std::round(t.font_size * 1.35f);
    return out;
}

float ApproximateMeasure::width(const std::string& utf8, float font_size) const {
    return ::tela::approximate_text_width(utf8, font_size);
}

} // namespace ergo::studio::tela_shell
