#include "thaleia_view.h"

#include "json_render.h"

namespace ergo::studio::views {

namespace {
constexpr float kDocWidth = 640;   // fits beside the tree in a 1280 px window with the sidebar
}

std::string inline_text(const JsonValue* inlines) {
    if (!inlines || !inlines->is_array()) return {};
    std::string out;
    for (const auto& in : *inlines->a) {
        const std::string kind = text_of(in, "kind");
        if (kind == "break") out += "\n";
        else if (kind == "code") out += "`" + text_of(in, "text") + "`";
        else out += text_of(in, "text");
    }
    return out;
}

ThaleiaView::ThaleiaView(host::IPlugin& plugin) : ExplorerView(plugin) {}

void ThaleiaView::on_first_show() { get("tree", "/api/tree"); }

void ThaleiaView::open(const std::string& path) {
    selected_ = path;
    get("doc", "/api/render?path=" + url_encode(path));
    touch();
}

void ThaleiaView::render(ui::ViewBuilder& b) {
    b.title(plugin_.info().title);
    b.muted(plugin_.info().description);
    {
        ui::ViewBuilder r = b.row(6);
        auto search = [this] {
            if (query_.empty()) return;
            get("search", "/api/search?q=" + url_encode(query_));
        };
        r.text_field(query_, "\xE5\x85\xA8\xE6\x96\x87\xE6\xA4\x9C\xE7\xB4\xA2", 360, search);   // 全文検索
        r.button("\xE6\xA4\x9C\xE7\xB4\xA2", search, true);                                       // 検索
        r.button("\xE5\x86\x8D\xE8\xAA\xAD\xE8\xBE\xBC", [this] {                                 // 再読込
            fetch("rescan", "POST", "/api/rescan", "{}", [this](const ApiResult&) { get("tree", "/api/tree"); });
        });
    }
    if (slot("search").loaded) {
        ui::ViewBuilder s = b.panel(3);
        s.muted("\xE6\xA4\x9C\xE7\xB4\xA2\xE7\xB5\x90\xE6\x9E\x9C");   // 検索結果
        if (slot_state(s, "search")) {
            const JsonValue& arr = slot("search").result.json;
            if (!arr.is_array() || arr.a->empty()) s.muted("\xE8\xA6\x8B\xE3\x81\xA4\xE3\x81\x8B\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93");   // 見つかりません
            else {
                std::size_t n = 0;
                for (const auto& d : *arr.a) {
                    if (++n > 40) break;
                    const std::string path = text_of(d, "path");
                    s.list_item(text_of(d, "title").empty() ? path : text_of(d, "title"), path == selected_,
                                [this, path] { open(path); }, text_of(d, "category"));
                }
            }
        }
    }
    ui::ViewBuilder row = b.row(12);
    row.node().layout.align_cross = ui_kit::AlignCross::Start;
    {
        ui::ViewBuilder tree = row.panel(2);
        tree.node().layout.w_sizing = ui_kit::Sizing::Fixed;
        tree.node().size.x          = 280;
        if (slot_state(tree, "tree")) {
            int budget = 400;   // nodes drawn at most (a huge tree must not stall the UI)
            const JsonValue& root = slot("tree").result.json;
            const JsonValue* kids = root.find("children");
            if (!kids || !kids->is_array() || kids->a->empty()) {
                tree.muted(text_of(root, "path"));
                tree.muted("\xE6\x96\x87\xE6\x9B\xB8\xE3\x81\x8C\xE3\x81\x82\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93");   // 文書がありません
            } else {
                for (const auto& c : *kids->a) render_tree(tree, c, 0, budget);
            }
        }
    }
    {
        ui::ViewBuilder doc = row.panel(4);
        doc.node().layout.w_sizing = ui_kit::Sizing::Fixed;
        doc.node().size.x          = kDocWidth + 20;
        render_document(doc);
    }
}

void ThaleiaView::render_tree(ui::ViewBuilder& b, const JsonValue& node, int depth, int& budget) {
    if (--budget < 0) return;
    const std::string path = text_of(node, "path");
    const std::string name = text_of(node, "name");
    const bool dir = node.find("is_directory") && node.find("is_directory")->as_bool();
    const std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
    if (dir) {
        const bool open_ = expanded_.count(path) > 0;
        b.list_item(indent + (open_ ? "\xE2\x96\xBE " : "\xE2\x96\xB8 ") + name, false, [this, path, open_] {   // ▾ / ▸
            if (open_) expanded_.erase(path);
            else expanded_.insert(path);
            touch();
        });
        if (!open_) return;
        if (const JsonValue* kids = node.find("children"); kids && kids->is_array()) {
            for (const auto& c : *kids->a) render_tree(b, c, depth + 1, budget);
        }
    } else {
        b.list_item(indent + "  " + name, path == selected_, [this, path] { open(path); });
    }
}

void ThaleiaView::render_document(ui::ViewBuilder& b) {
    if (selected_.empty()) {
        b.muted("\xE5\xB7\xA6\xE3\x81\xAE\xE4\xB8\x80\xE8\xA6\xA7\xE3\x81\x8B\xE3\x82\x89\xE6\x96\x87\xE6\x9B\xB8\xE3\x82\x92\xE9\x81\xB8\xE3\x81\xB6");   // 左の一覧から文書を選ぶ
        return;
    }
    b.muted(selected_);
    if (!slot_state(b, "doc")) return;
    const JsonValue& r = slot("doc").result.json;
    const JsonValue* blocks = r.find("blocks");
    if (!blocks || !blocks->is_array()) {
        render_json(b, r);
        return;
    }
    const ui::Theme& t = b.theme();
    for (const auto& bl : *blocks->a) {
        const std::string kind = text_of(bl, "kind");
        if (kind == "heading") {
            const double lvl = bl.find("level") ? bl.find("level")->as_number(1) : 1;
            const float size = lvl <= 1 ? t.title_size + 2 : lvl == 2 ? t.title_size : t.font_size + 1;
            b.spacer(4);
            b.wrapped(inline_text(bl.find("inlines")), kDocWidth, &t.accent, size);
        } else if (kind == "paragraph") {
            b.wrapped(inline_text(bl.find("inlines")), kDocWidth);
        } else if (kind == "list_item") {
            const int lvl = bl.find("level") ? static_cast<int>(bl.find("level")->as_number(0)) : 0;
            std::string bullet = bl.find("ordered") && bl.find("ordered")->as_bool() ? text_of(bl, "number") + ". " : "\xE2\x80\xA2 ";
            if (const JsonValue* c = bl.find("checked")) bullet = c->as_bool() ? "\xE2\x98\x91 " : "\xE2\x98\x90 ";
            b.wrapped(std::string(static_cast<std::size_t>(lvl) * 2, ' ') + bullet + inline_text(bl.find("inlines")), kDocWidth);
        } else if (kind == "code") {
            ui::ViewBuilder c = b.panel(1);
            c.node().style.fill = t.background;
            for (const auto& l : ui::wrap_text(text_of(bl, "code"), kDocWidth - 20, t.small_size, b.measure())) {
                c.label(l, &t.muted, t.small_size);
            }
        } else if (kind == "quote") {
            ui::ViewBuilder q = b.panel(2);
            if (const JsonValue* kids = bl.find("children"); kids && kids->is_array()) {
                for (const auto& k : *kids->a) q.wrapped(inline_text(k.find("inlines")), kDocWidth - 30, &t.muted);
            }
        } else if (kind == "table") {
            ui::ViewBuilder tb = b.panel(2);
            if (const JsonValue* rows = bl.find("rows"); rows && rows->is_array()) {
                for (const auto& row : *rows->a) {
                    if (!row.is_array()) continue;
                    ui::ViewBuilder r = tb.row(14);
                    const float cell = row.a->empty() ? kDocWidth : (kDocWidth - 20) / static_cast<float>(row.a->size());
                    for (const auto& c : *row.a) r.clipped_label(inline_text(&c), cell - 14);
                }
            }
        } else if (kind == "rule") {
            b.separator();
        }
    }
}

} // namespace ergo::studio::views
