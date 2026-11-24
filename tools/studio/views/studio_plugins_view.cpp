#include "studio_view.h"

#include "json_render.h"

#include <algorithm>

namespace ergo::studio::views {
namespace {

constexpr char kRegistry[] = "/api/plugins/registry";
constexpr char kPacks[] = "/api/plugins";

bool matches_tag(const JsonValue& item, const char* key, const std::string& filter, bool empty_matches) {
    if (filter.empty()) return true;
    const auto* tags = item.find(key);
    if (!tags || !tags->is_array() || tags->a->empty()) return empty_matches;
    return std::any_of(tags->a->begin(), tags->a->end(), [&](const auto& tag) {
        return tag.is_string() && tag.s == filter;
    });
}

JsonValue body(const char* key, const std::string& value) {
    auto out = JsonValue::make_object();
    out.set(key, JsonValue::make_string(value));
    return out;
}

} // namespace

bool StudioView::render_published(ui::ViewBuilder& b) {
    render_plugins(b, false);
    return true;
}

void StudioView::render_plugins(ui::ViewBuilder& b, bool local) {
    b.title("プラグイン");
    ui::ViewBuilder filters = b.row(6);
    auto& domains = local ? domains_ : published_domains_;
    auto& styles = local ? styles_ : published_styles_;
    filters.text_field(domains, "用途", 140);
    filters.text_field(styles, "様式", 140);
    filters.button("索引を更新", [this] { if (!slot("registry").loading) get("registry", kRegistry); });
    if (local) render_installed(b);
    if (!slot_state(b, "registry")) return;
    const auto& registry = slot("registry").result.json;
    const auto reason = text_of(registry, "reason");
    if (!reason.empty()) b.label("信頼できない · " + reason, &b.theme().warn);
    const auto* entries = registry.find("plugins");
    if (!entries || !entries->is_array()) return;
    for (const auto& entry : *entries->a) {
        if (!matches_tag(entry, "domains", domains, false) || !matches_tag(entry, "styles", styles, true)) continue;
        const auto id = text_of(entry, "id");
        const auto* value = entry.find("trusted");
        const bool trusted = value && value->as_bool();
        auto row = b.panel(4);
        row.label(text_of(entry, "title"));
        row.muted(id);
        row.label(trusted ? "信頼できる" : "信頼できない · " + text_of(entry, "reason"),
                  trusted ? &b.theme().ok : &b.theme().warn);
        if (local && trusted && text_of(entry, "kind") == "pack") {
            row.button("導入をプレビュー", [this, id] {
                if (!slot("plugin-preview").loading)
                    fetch("plugin-preview", "POST", std::string(kRegistry) + "/preview",
                          ergo::common::jsonm::serialize(body("id", id)));
            });
        }
    }
    if (local && slot("plugin-preview").loaded) {
        const auto& result = slot("plugin-preview").result;
        if (!result.ok()) b.label(result.error_text(), &b.theme().error);
        if (const auto* preview = result.json.find("preview")) {
            const auto token = text_of(*preview, "token");
            b.field("取得元", text_of(*preview, "id"));
            b.field("コミット", text_of(*preview, "rev"));
            if (!token.empty()) b.button("入れる", [this, token] {
                post("/api/plugins/install", body("token", token), "導入しました。次回起動から有効です", "packs", kPacks);
            }, true);
        }
    }
}

void StudioView::render_installed(ui::ViewBuilder& b) {
    b.button("導入済みを更新", [this] { if (!slot("packs").loading) get("packs", kPacks); });
    if (!slot_state(b, "packs")) return;
    const auto* packs = slot("packs").result.json.find("packs");
    if (!packs || !packs->is_array()) return;
    for (const auto& pack : *packs->a) {
        auto row = b.row(6);
        const auto id = text_of(pack, "id");
        const auto* value = pack.find("enabled");
        const bool enabled = value && value->as_bool();
        row.label(id);
        row.button(enabled ? "無効にする" : "有効にする", [this, id, enabled] {
            auto request = body("id", id);
            request.set("enabled", JsonValue::make_bool(!enabled));
            post("/api/plugins/enable", request, "設定を保存しました。次回起動から反映します", "packs", kPacks);
        });
        row.button("外す", [this, id] {
            fetch("action", "DELETE", std::string(kPacks) + "?id=" + url_encode(id), {}, [this](const ApiResult& result) {
                set_notice(result.ok() ? "外しました。次回起動から反映します" : result.error_text(), result.ok());
                if (result.ok()) get("packs", kPacks);
            });
        });
    }
}

} // namespace ergo::studio::views
