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

ViewBuilder::ViewBuilder(Node& container, std::string container_path, const ITextMeasure& measure, const Theme& theme,
                         std::vector<ClickBinding>& clicks)
    : root_(container), path_(std::move(container_path)), measure_(measure), theme_(theme), clicks_(clicks) {}

ViewBuilder::ViewBuilder(Node& root, std::vector<std::size_t> index, std::string path, const ITextMeasure& measure,
                         const Theme& theme, std::vector<ClickBinding>& clicks)
    : root_(root), index_(std::move(index)), path_(std::move(path)), measure_(measure), theme_(theme), clicks_(clicks) {}

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
    return ViewBuilder(root_, std::move(idx), path, measure_, theme_, clicks_);
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
    if (on_click) clicks_.push_back({path, std::move(on_click)});
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
