#include "asset_import_view.h"

#include "json_render.h"

namespace ergo::studio::views {
namespace {
constexpr auto kRefreshInterval = std::chrono::seconds(5);
}

uint64_t AssetImportView::view_revision() const {
    const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    return ApiView::view_revision() + static_cast<uint64_t>(elapsed / kRefreshInterval);
}

void AssetImportView::on_first_show() {
    refreshed_ = std::chrono::steady_clock::now();
    get("list", "/api/list");
    if (!selected_.empty()) get("detail", "/api/detail?name=" + url_encode(selected_));
}

void AssetImportView::action(const std::string& path) {
    fetch("action", "POST", path, "{}", [this](const ApiResult& result) {
        const auto status = text_of(result.json, "status");
        set_notice(!result.ok() ? result.error_text() : status == "queued" ? "モデル表示窓に依頼しました" :
                   status == "uncaptured" ? "未撮影 (モデル表示窓がありません)" : "取り込みを確認しました", result.ok());
        get("list", "/api/list");
    });
}

void AssetImportView::render(ui::ViewBuilder& b) {
    if (!slot("list").loading && std::chrono::steady_clock::now() - refreshed_ >= kRefreshInterval)
        on_first_show();
    b.title("Visus 一覧");
    auto actions = b.row();
    actions.button("更新", [this] { on_first_show(); });
    actions.button("取り込みを確認", [this] { action("/api/import"); }, false, true);
    render_notice(b);
    if (!slot_state(b, "list")) return;
    const auto& list = slot("list").result.json;
    const auto* items = list.find("items");
    if (!items || !items->is_array() || items->a->empty()) b.muted("登録された Visus がありません");
    else for (const auto& item : *items->a) {
        const auto name = text_of(item, "name");
        auto card = b.panel();
        const auto revision = text_of(item, "revision");
        const auto thumbnail = text_of(item, "thumbnail");
        if (!thumbnail.empty()) {
            // Tela represents external media by a named rectangle in the browser frame.
            card.video(name, "visus:" + thumbnail, 180, 180);
        } else {
            card.muted("未撮影");
        }
        card.list_item(name, name == selected_, [this, name] {
            selected_ = name;
            get("detail", "/api/detail?name=" + url_encode(name));
        }, text_of(item, "kind") + " " + revision.substr(0, 12));
        if (const auto* children = item.find("children"); children && children->is_array()) {
            for (const auto& child : *children->a) card.muted("子 " + child.as_string());
        }
    }
    const auto* issues = list.find("issues");
    if (issues && issues->is_array() && !issues->a->empty()) {
        b.title("取り込みでスキップしたファイル");
        for (const auto& issue : *issues->a) b.field(text_of(issue, "source"), text_of(issue, "reason"));
    }
    if (selected_.empty() || !slot_state(b, "detail")) return;
    b.separator();
    b.title(selected_);
    if (list.find("window") && list.find("window")->as_bool()) {
        auto buttons = b.row();
        const auto key = "?name=" + url_encode(selected_);
        buttons.button("モデル表示窓で開く", [this, key] { action("/api/preview" + key); }, false, true);
        buttons.button("撮影", [this, key] { action("/api/capture" + key); }, false, true);
    }
    render_json(b, slot("detail").result.json, {7, 50, 900});
}

} // namespace ergo::studio::views
