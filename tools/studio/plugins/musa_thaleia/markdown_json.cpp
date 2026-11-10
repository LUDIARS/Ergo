#include "markdown.h"

namespace ergo::studio::plugins::musa_thaleia {
namespace {

const char* kind_name(InlineKind k) {
    switch (k) {
        case InlineKind::Code: return "code";
        case InlineKind::Link: return "link";
        case InlineKind::Image: return "image";
        case InlineKind::Break: return "break";
        default: return "text";
    }
}

const char* kind_name(BlockKind k) {
    switch (k) {
        case BlockKind::Heading: return "heading";
        case BlockKind::Code: return "code";
        case BlockKind::Quote: return "quote";
        case BlockKind::ListItem: return "list_item";
        case BlockKind::Rule: return "rule";
        case BlockKind::Table: return "table";
        default: return "paragraph";
    }
}

const char* align_name(Align a) {
    switch (a) {
        case Align::Left: return "left";
        case Align::Center: return "center";
        case Align::Right: return "right";
        default: return "";
    }
}

JsonValue inlines_json(const std::vector<Inline>& v) {
    JsonValue arr = JsonValue::make_array();
    for (const auto& in : v) {
        JsonValue o = jobj({{"kind", jstr(kind_name(in.kind))}, {"text", jstr(in.text)}});
        if (!in.url.empty()) o.set("url", jstr(in.url));
        if (in.strong) o.set("strong", jbool(true));
        if (in.em) o.set("em", jbool(true));
        if (in.strike) o.set("strike", jbool(true));
        arr.push(std::move(o));
    }
    return arr;
}

JsonValue block_json(const Block& b) {
    JsonValue o = jobj({{"kind", jstr(kind_name(b.kind))}});
    switch (b.kind) {
        case BlockKind::Heading:
            o.set("level", jnum(b.level));
            o.set("anchor", jstr(b.anchor));
            o.set("inlines", inlines_json(b.inlines));
            break;
        case BlockKind::Paragraph: o.set("inlines", inlines_json(b.inlines)); break;
        case BlockKind::Code:
            o.set("lang", jstr(b.lang));
            o.set("code", jstr(b.code));
            break;
        case BlockKind::Quote: o.set("children", to_json_value(b.children)); break;
        case BlockKind::ListItem:
            o.set("level", jnum(b.level));
            o.set("ordered", jbool(b.ordered));
            if (b.ordered) o.set("number", jnum(b.number));
            if (b.checked) o.set("checked", jbool(*b.checked));
            o.set("inlines", inlines_json(b.inlines));
            break;
        case BlockKind::Table: {
            JsonValue aligns = JsonValue::make_array();
            for (auto a : b.aligns) aligns.push(jstr(align_name(a)));
            JsonValue rows = JsonValue::make_array();
            for (const auto& r : b.rows) {
                JsonValue row = JsonValue::make_array();
                for (const auto& cell : r) row.push(inlines_json(cell));
                rows.push(std::move(row));
            }
            o.set("aligns", aligns);
            o.set("rows", rows);
            break;
        }
        case BlockKind::Rule: break;
    }
    return o;
}

} // namespace

JsonValue to_json_value(const std::vector<Block>& blocks) {
    JsonValue arr = JsonValue::make_array();
    for (const auto& b : blocks) arr.push(block_json(b));
    return arr;
}

JsonValue to_json_value(const std::vector<TocEntry>& toc) {
    JsonValue arr = JsonValue::make_array();
    for (const auto& t : toc) arr.push(jobj({{"level", jnum(t.level)}, {"title", jstr(t.title)}, {"anchor", jstr(t.anchor)}}));
    return arr;
}

} // namespace ergo::studio::plugins::musa_thaleia
