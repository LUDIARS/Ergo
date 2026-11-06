#include "ergo/studio/ui/view_builder.h"

namespace ergo::studio::ui {

using ui_kit::Color;
using ui_kit::LayoutMode;
using ui_kit::Node;
using ui_kit::NodeKind;
using ui_kit::Sizing;

float FixedAdvanceMeasure::width(const std::string& utf8, float font_size) const {
    std::size_t cps = 0;
    for (unsigned char c : utf8) {
        if ((c & 0xC0) != 0x80) ++cps;
    }
    return static_cast<float>(cps) * advance_ * font_size;
}

namespace {
/// Byte offsets where each UTF-8 code point starts (plus the end).
std::vector<std::size_t> cp_starts(const std::string& s) {
    std::vector<std::size_t> v;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) v.push_back(i);
    }
    v.push_back(s.size());
    return v;
}
const char* const kEllipsis = "…";   // ellipsis
} // namespace

std::string fit_text(const std::string& text, float max_width, float font_size, const ITextMeasure& m) {
    if (m.width(text, font_size) <= max_width) return text;
    const auto starts = cp_starts(text);
    // Binary search the longest prefix (in code points) that fits with the ellipsis.
    std::size_t lo = 0, hi = starts.size() - 1;
    while (lo < hi) {
        const std::size_t mid = (lo + hi + 1) / 2;
        if (m.width(text.substr(0, starts[mid]) + kEllipsis, font_size) <= max_width) lo = mid;
        else hi = mid - 1;
    }
    return text.substr(0, starts[lo]) + kEllipsis;
}

std::vector<std::string> wrap_text(const std::string& text, float max_width, float font_size, const ITextMeasure& m) {
    std::vector<std::string> out;
    std::size_t para = 0;
    while (para <= text.size()) {
        const auto nl = text.find('\n', para);
        const std::string line = text.substr(para, nl == std::string::npos ? std::string::npos : nl - para);
        if (m.width(line, font_size) <= max_width) {
            out.push_back(line);
        } else {
            const auto starts = cp_starts(line);
            std::size_t begin = 0;        // byte offset of the current output line
            std::size_t last_space = 0;   // byte offset just after the last space in it (0 = none)
            for (std::size_t i = 1; i < starts.size(); ++i) {
                const std::size_t end = starts[i];
                if (line[starts[i - 1]] == ' ') last_space = end;
                if (m.width(line.substr(begin, end - begin), font_size) <= max_width) continue;
                // starts[i-1] does not fit: break before it (at the last space when there is one).
                std::size_t cut = last_space > begin ? last_space : starts[i - 1];
                if (cut <= begin) cut = end;   // one code point wider than the line: keep it alone
                out.push_back(line.substr(begin, cut - begin));
                begin      = cut;
                last_space = 0;
            }
            if (begin < line.size()) out.push_back(line.substr(begin));
        }
        if (nl == std::string::npos) break;
        para = nl + 1;
    }
    return out;
}

std::string fit_tail(const std::string& text, float max_width, float font_size, const ITextMeasure& m) {
    if (m.width(text, font_size) <= max_width) return text;
    const auto starts = cp_starts(text);
    std::size_t lo = 0, hi = starts.size() - 1;   // index of the first code point kept
    while (lo < hi) {
        const std::size_t mid = (lo + hi) / 2;
        if (m.width(text.substr(starts[mid]), font_size) <= max_width) hi = mid;
        else lo = mid + 1;
    }
    return text.substr(starts[lo]);
}

ViewBuilder::ViewBuilder(Node& container, std::string container_path, const ITextMeasure& measure, const Theme& theme,
                         Bindings& bindings)
    : root_(container), path_(std::move(container_path)), measure_(measure), theme_(theme), bind_(bindings) {}

ViewBuilder::ViewBuilder(Node& root, std::vector<std::size_t> index, std::string path, const ITextMeasure& measure,
                         const Theme& theme, Bindings& bindings)
    : root_(root), index_(std::move(index)), path_(std::move(path)), measure_(measure), theme_(theme), bind_(bindings) {}

Node& ViewBuilder::node() {
    Node* n = &root_;
    for (std::size_t i : index_) n = &n->children[i];
    return *n;
}

std::string ViewBuilder::next_name(const char* kind) { return std::string(kind) + std::to_string(counter_++); }

void ViewBuilder::add(Node n) { node().children.push_back(std::move(n)); }

ViewBuilder ViewBuilder::child(Node n) {
    const std::string path = path_ + "/" + n.name;
    Node& me = node();
    me.children.push_back(std::move(n));
    std::vector<std::size_t> idx = index_;
    idx.push_back(me.children.size() - 1);
    return ViewBuilder(root_, std::move(idx), path, measure_, theme_, bind_);
}

namespace {
Node text_node(std::string name, const std::string& text, float size, const Color& color, const ITextMeasure& m) {
    Node t;
    t.name             = std::move(name);
    t.kind             = NodeKind::Text;
    t.text             = text;
    t.style.font_size  = size;
    t.style.text_color = color;
    t.size             = {m.width(text, size), m.line_height(size)};
    return t;
}
} // namespace

void ViewBuilder::title(const std::string& text) {
    add(text_node(next_name("title"), text, theme_.title_size, theme_.text, measure_));
}

void ViewBuilder::label(const std::string& text, const Color* color, float font_size) {
    add(text_node(next_name("label"), text, font_size > 0 ? font_size : theme_.font_size, color ? *color : theme_.text,
                  measure_));
}

void ViewBuilder::muted(const std::string& text) {
    add(text_node(next_name("muted"), text, theme_.small_size, theme_.muted, measure_));
}

void ViewBuilder::paragraph(const std::string& text) {
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        label(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
}

void ViewBuilder::button(const std::string& text, ClickFn on_click, bool primary) {
    Node b;
    b.name                = next_name("button");
    b.kind                = NodeKind::Frame;
    b.style.fill          = primary ? theme_.accent : theme_.panel_alt;
    b.style.corner_radius = 4;
    b.layout.mode         = LayoutMode::Horizontal;
    b.layout.pad_l = b.layout.pad_r = 12;
    b.layout.pad_t = b.layout.pad_b = 6;
    b.layout.w_sizing = b.layout.h_sizing = Sizing::Hug;
    b.children.push_back(text_node("text", text, theme_.font_size, theme_.text, measure_));
    const std::string path = path_ + "/" + b.name;
    add(std::move(b));
    if (on_click) bind_.clicks.push_back({path, std::move(on_click)});
}

void ViewBuilder::field(const std::string& key, const std::string& value, const Color* value_color) {
    ViewBuilder r = row(6);
    r.label(key + ":", &theme_.muted);
    r.label(value, value_color);
}

void ViewBuilder::status(bool ok, const std::string& text) {
    ViewBuilder r = row(8);
    Node dot;
    dot.name                = "dot";
    dot.kind                = NodeKind::Shape;
    dot.size                = {10, 10};
    dot.style.fill          = ok ? theme_.ok : theme_.error;
    dot.style.corner_radius = 5;
    r.add(std::move(dot));
    r.label(text);
}

void ViewBuilder::separator() {
    Node s;
    s.name            = next_name("sep");
    s.kind            = NodeKind::Shape;
    s.size            = {1, 1};
    s.style.fill      = theme_.panel_alt;
    s.layout.w_sizing = Sizing::Fill;
    add(std::move(s));
}

void ViewBuilder::spacer(float h) {
    Node s;
    s.name = next_name("space");
    s.size = {1, h};
    add(std::move(s));
}

void ViewBuilder::field_frame(std::string& value, const std::string& placeholder, float width, int lines,
                              bool multiline, ClickFn on_submit, bool masked) {
    Node f;
    f.name                = next_name("field");
    f.kind                = NodeKind::Frame;
    f.layout.mode         = LayoutMode::Vertical;
    f.layout.pad_l = f.layout.pad_r = 8;
    f.layout.pad_t = f.layout.pad_b = 5;
    f.clip                = true;
    const std::string path = path_ + "/" + f.name;
    const bool focused     = bind_.focused == path;
    f.style.fill           = focused ? theme_.panel : theme_.panel_alt;
    f.style.corner_radius  = 4;
    const float lh         = measure_.line_height(theme_.font_size);
    f.size                 = {width, lh * static_cast<float>(lines) + 10};
    const float inner      = width - 16;
    const std::string caret = focused ? "|" : "";
    if (value.empty() && !focused) {
        f.children.push_back(text_node("ph", fit_text(placeholder, inner, theme_.font_size, measure_), theme_.font_size,
                                       theme_.muted, measure_));
    } else if (masked) {
        std::string dots;
        for (unsigned char c : value) {
            if ((c & 0xC0) != 0x80) dots += "\xE2\x80\xA2";   // one bullet per code point
        }
        f.children.push_back(text_node("text", fit_tail(dots + caret, inner, theme_.font_size, measure_), theme_.font_size,
                                       theme_.text, measure_));
    } else if (!multiline) {
        f.children.push_back(text_node("text", fit_tail(value + caret, inner, theme_.font_size, measure_), theme_.font_size,
                                       theme_.text, measure_));
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
        const std::size_t first = ls.size() > static_cast<std::size_t>(lines) ? ls.size() - lines : 0;
        for (std::size_t i = first; i < ls.size(); ++i) {
            f.children.push_back(text_node("line" + std::to_string(i), fit_tail(ls[i], inner, theme_.font_size, measure_),
                                           theme_.font_size, theme_.text, measure_));
        }
    }
    add(std::move(f));
    bind_.fields.push_back({path, &value, std::move(on_submit), multiline});
    // Clicking a field focuses it (the app owns focus; this only names the path).
    Bindings* b = &bind_;
    bind_.clicks.push_back({path, [b, path] { b->focused = path; }});
}

void ViewBuilder::text_field(std::string& value, const std::string& placeholder, float width, ClickFn on_submit) {
    field_frame(value, placeholder, width, 1, false, std::move(on_submit));
}

void ViewBuilder::secret_field(std::string& value, const std::string& placeholder, float width) {
    field_frame(value, placeholder, width, 1, false, {}, true);
}

void ViewBuilder::text_area(std::string& value, float width, int lines) {
    field_frame(value, {}, width, lines < 1 ? 1 : lines, true, {});
}

void ViewBuilder::list_item(const std::string& text, bool selected, ClickFn on_click, const std::string& detail) {
    Node it;
    it.name                = next_name("item");
    it.kind                = NodeKind::Frame;
    it.layout.mode         = LayoutMode::Horizontal;
    it.layout.gap          = 10;
    it.layout.align_cross  = ui_kit::AlignCross::Center;
    it.layout.pad_l = it.layout.pad_r = 8;
    it.layout.pad_t = it.layout.pad_b = 4;
    it.layout.w_sizing     = Sizing::Fill;
    it.layout.h_sizing     = Sizing::Hug;
    it.style.fill          = selected ? theme_.accent : theme_.panel_alt;
    it.style.corner_radius = 3;
    it.children.push_back(text_node("text", text, theme_.font_size, theme_.text, measure_));
    if (!detail.empty()) {
        it.children.push_back(text_node("detail", detail, theme_.small_size, selected ? theme_.text : theme_.muted, measure_));
    }
    const std::string path = path_ + "/" + it.name;
    add(std::move(it));
    if (on_click) bind_.clicks.push_back({path, std::move(on_click)});
}

void ViewBuilder::wrapped(const std::string& text, float max_width, const Color* color, float font_size) {
    const float size = font_size > 0 ? font_size : theme_.font_size;
    for (const auto& l : wrap_text(text, max_width, size, measure_)) {
        add(text_node(next_name("line"), l, size, color ? *color : theme_.text, measure_));
    }
}

void ViewBuilder::clipped_label(const std::string& text, float max_width, const Color* color) {
    add(text_node(next_name("label"), fit_text(text, max_width, theme_.font_size, measure_), theme_.font_size,
                  color ? *color : theme_.text, measure_));
}

ViewBuilder ViewBuilder::row(float gap) {
    Node r;
    r.name               = next_name("row");
    r.layout.mode        = LayoutMode::Horizontal;
    r.layout.gap         = gap;
    r.layout.align_cross = ui_kit::AlignCross::Center;
    r.layout.w_sizing = r.layout.h_sizing = Sizing::Hug;
    return child(std::move(r));
}

ViewBuilder ViewBuilder::panel(float gap) {
    Node p;
    p.name                = next_name("panel");
    p.layout.mode         = LayoutMode::Vertical;
    p.layout.gap          = gap;
    p.layout.pad_l = p.layout.pad_r = p.layout.pad_t = p.layout.pad_b = 10;
    p.layout.w_sizing     = Sizing::Fill;
    p.layout.h_sizing     = Sizing::Hug;
    p.style.fill          = theme_.panel;
    p.style.corner_radius = 6;
    return child(std::move(p));
}

} // namespace ergo::studio::ui
