#include "ergo/studio/ui/view_builder.h"

namespace ergo::studio::ui {

using ui_kit::Color;

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

ViewBuilder::ViewBuilder(ViewNode& container, const ITextMeasure& measure, const Theme& theme)
    : root_(container), measure_(measure), theme_(theme) {}

ViewBuilder::ViewBuilder(ViewNode& root, std::vector<std::size_t> index, const ITextMeasure& measure, const Theme& theme)
    : root_(root), index_(std::move(index)), measure_(measure), theme_(theme) {}

ViewNode& ViewBuilder::node() {
    ViewNode* n = &root_;
    for (std::size_t i : index_) n = &n->children[i];
    return *n;
}

std::string ViewBuilder::next_name(const char* kind) {
    std::string n = std::string(kind) + std::to_string(counter_++);
    if (!pending_name_.empty()) n = std::move(pending_name_);
    pending_name_.clear();
    return n;
}

void ViewBuilder::name_next(std::string name) { pending_name_ = std::move(name); }

void ViewBuilder::add(ViewNode n) { node().children.push_back(std::move(n)); }

ViewBuilder ViewBuilder::child(ViewNode n) {
    ViewNode& me = node();
    me.children.push_back(std::move(n));
    std::vector<std::size_t> idx = index_;
    idx.push_back(me.children.size() - 1);
    return ViewBuilder(root_, std::move(idx), measure_, theme_);
}

ViewNode ViewBuilder::text_node(const char* kind, const std::string& text, float size, const Color& color) {
    ViewNode t;
    t.kind      = ViewKind::Text;
    t.name      = next_name(kind);
    t.text      = text;
    t.font_size = size;
    t.color     = color;
    return t;
}

void ViewBuilder::title(const std::string& text) { add(text_node("title", text, theme_.title_size, theme_.text)); }

void ViewBuilder::label(const std::string& text, const Color* color, float font_size) {
    add(text_node("label", text, font_size > 0 ? font_size : theme_.font_size, color ? *color : theme_.text));
}

void ViewBuilder::muted(const std::string& text) { add(text_node("muted", text, theme_.small_size, theme_.muted)); }

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
    ViewNode b;
    b.kind     = ViewKind::Button;
    b.name     = next_name("button");
    b.text     = text;
    b.primary  = primary;
    b.on_click = std::move(on_click);
    add(std::move(b));
}

void ViewBuilder::field(const std::string& key, const std::string& value, const Color* value_color) {
    ViewBuilder r = row(6);
    r.label(key + ":", &theme_.muted);
    r.label(value, value_color);
}

void ViewBuilder::status(bool ok, const std::string& text) {
    ViewNode s;
    s.kind = ViewKind::Status;
    s.name = next_name("status");
    s.text = text;
    s.ok   = ok;
    add(std::move(s));
}

void ViewBuilder::separator() {
    ViewNode s;
    s.kind = ViewKind::Separator;
    s.name = next_name("sep");
    add(std::move(s));
}

void ViewBuilder::spacer(float h) {
    ViewNode s;
    s.kind   = ViewKind::Spacer;
    s.name   = next_name("space");
    s.height = h;
    add(std::move(s));
}

void ViewBuilder::text_field(std::string& value, const std::string& placeholder, float width, ClickFn on_submit) {
    ViewNode f;
    f.kind      = ViewKind::Field;
    f.name      = next_name("field");
    f.text      = placeholder;
    f.width     = width;
    f.value     = &value;
    f.on_submit = std::move(on_submit);
    add(std::move(f));
}

void ViewBuilder::secret_field(std::string& value, const std::string& placeholder, float width) {
    ViewNode f;
    f.kind   = ViewKind::Field;
    f.name   = next_name("field");
    f.text   = placeholder;
    f.width  = width;
    f.value  = &value;
    f.secret = true;
    add(std::move(f));
}

void ViewBuilder::text_area(std::string& value, float width, int lines) {
    ViewNode f;
    f.kind      = ViewKind::Field;
    f.name      = next_name("field");
    f.width     = width;
    f.lines     = lines < 1 ? 1 : lines;
    f.multiline = true;
    f.value     = &value;
    add(std::move(f));
}

void ViewBuilder::list_item(const std::string& text, bool selected, ClickFn on_click, const std::string& detail) {
    ViewNode it;
    it.kind     = ViewKind::ListItem;
    it.name     = next_name("item");
    it.text     = text;
    it.detail   = detail;
    it.selected = selected;
    it.on_click = std::move(on_click);
    add(std::move(it));
}

void ViewBuilder::slider(float fraction, SetFractionFn on_set, float width) {
    ViewNode s;
    s.kind     = ViewKind::Slider;
    s.name     = next_name("slider");
    s.fraction = fraction < 0 ? 0.0f : (fraction > 1 ? 1.0f : fraction);
    s.width    = width;
    s.on_set   = std::move(on_set);
    add(std::move(s));
}

void ViewBuilder::wrapped(const std::string& text, float max_width, const Color* color, float font_size) {
    const float size = font_size > 0 ? font_size : theme_.font_size;
    for (const auto& l : wrap_text(text, max_width, size, measure_)) {
        add(text_node("line", l, size, color ? *color : theme_.text));
    }
}

void ViewBuilder::clipped_label(const std::string& text, float max_width, const Color* color) {
    add(text_node("label", fit_text(text, max_width, theme_.font_size, measure_), theme_.font_size,
                  color ? *color : theme_.text));
}

void ViewBuilder::video(const std::string& label, const std::string& stream, float width, float height) {
    ViewNode v;
    v.kind   = ViewKind::Video;
    v.name   = next_name("video");
    v.text   = label;
    v.detail = stream;
    v.width  = width;
    v.height = height;
    add(std::move(v));
}

ViewBuilder ViewBuilder::row(float gap) {
    ViewNode r;
    r.kind = ViewKind::Row;
    r.name = next_name("row");
    r.gap  = gap;
    return child(std::move(r));
}

ViewBuilder ViewBuilder::panel(float gap) {
    ViewNode p;
    p.kind = ViewKind::Panel;
    p.name = next_name("panel");
    p.gap  = gap;
    return child(std::move(p));
}

} // namespace ergo::studio::ui
