#include "vfx_suggest_view.h"

#include "json_render.h"

#include <cstdlib>

namespace ergo::studio::views {
namespace jm = ergo::common::jsonm;

VfxSuggestView::VfxSuggestView(host::IPlugin& plugin, host::IPlugin* particle)
    : ExplorerView(plugin), particle_(particle ? std::make_unique<PluginApi>(*particle) : nullptr) {}

void VfxSuggestView::on_first_show() {
    get("games", "/api/games");
    get("health", "/api/health");
}

void VfxSuggestView::generate() {
    if (scene_.empty()) {
        set_notice("\xE3\x82\xB7\xE3\x83\xBC\xE3\x83\xB3\xE3\x81\xAE\xE8\xAA\xAC\xE6\x98\x8E\xE3\x81\x8C\xE8\xA6\x81\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x99", false);   // シーンの説明が要ります
        return;
    }
    JsonValue body = JsonValue::make_object();
    body.set("game", JsonValue::make_string(game_));
    body.set("scene", JsonValue::make_string(scene_));
    char* end = nullptr;
    const long n = std::strtol(count_.c_str(), &end, 10);
    if (end != count_.c_str() && n > 0) body.set("count", JsonValue::make_number(static_cast<double>(n)));
    open_json_.clear();
    fetch("generated", "POST", "/api/generate", jm::serialize(body), [this](const ApiResult& r) {
        if (!r.ok()) set_notice(r.error_text(), false);
    });
}

void VfxSuggestView::send_to_particle(const JsonValue& config, const std::string& name) {
    if (!particle_) return;
    const std::string body = jm::serialize(config);
    particle_->call("POST", "/api/effect", body, [this, name](ApiResult r) {
        set_notice(r.ok() ? "particle \xE3\x81\xB8\xE9\x80\x81\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F: " + name : r.error_text(), r.ok());   // particle へ送りました
    });
}

void VfxSuggestView::render(ui::ViewBuilder& b) {
    b.title(plugin_.info().title);
    b.muted(plugin_.info().description);
    {
        const Slot& h = slot("health");
        if (h.loaded && !(h.result.json.find("ok") && h.result.json.find("ok")->as_bool())) {
            b.label("LLM: " + (text_of(h.result.json, "err").empty() ? text_of(h.result.json, "error") : text_of(h.result.json, "err")),
                    &b.theme().warn);
        }
    }
    render_notice(b);
    ui::ViewBuilder f = b.panel(5);
    {
        ui::ViewBuilder r = f.row(6);
        const Slot& g = slot("games");
        if (g.loaded && g.result.json.is_array()) {
            for (const auto& x : *g.result.json.a) {
                const std::string id = text_of(x, "id");
                r.button(text_of(x, "label"), [this, id] {
                    game_ = id;
                    touch();
                }, id == game_);
            }
        }
    }
    f.muted("\xE3\x82\xB7\xE3\x83\xBC\xE3\x83\xB3 (\xE4\xBE\x8B: \xE5\xA4\x9C\xE3\x81\xAE\xE6\xA3\xAE\xE3\x81\xA7\xE8\x9B\x8D\xE3\x81\x8C\xE9\xA3\x9B\xE3\x81\xB6)");   // シーン (例: 夜の森で蛍が飛ぶ)
    f.text_area(scene_, 760, 3);
    {
        ui::ViewBuilder r = f.row(6);
        r.muted("\xE5\x80\x8B\xE6\x95\xB0");   // 個数
        r.text_field(count_, "3", 60);
        r.button("\xE7\x94\x9F\xE6\x88\x90", [this] { generate(); }, true);                         // 生成
        r.button("\xE3\x82\xAB\xE3\x82\xBF\xE3\x83\xAD\xE3\x82\xB0", [this] { get("catalog", "/api/catalog"); });   // カタログ
    }
    const Slot& gen = slot("generated");
    if (gen.loading || gen.loaded) {
        ui::ViewBuilder p = b.panel(4);
        p.muted("\xE7\x94\x9F\xE6\x88\x90\xE7\xB5\x90\xE6\x9E\x9C");   // 生成結果
        if (slot_state(p, "generated")) {
            if (const JsonValue* list = gen.result.json.find("presets")) render_presets(p, "generated", *list);
        }
    }
    if (slot("catalog").loaded) {
        ui::ViewBuilder p = b.panel(4);
        p.muted("\xE3\x82\xAB\xE3\x82\xBF\xE3\x83\xAD\xE3\x82\xB0");   // カタログ
        if (slot_state(p, "catalog")) render_presets(p, "catalog", slot("catalog").result.json);
    }
}

void VfxSuggestView::render_presets(ui::ViewBuilder& b, const char* slot_name, const JsonValue& list) {
    if (!list.is_array()) return;
    for (std::size_t i = 0; i < list.a->size(); ++i) {
        const JsonValue& p = (*list.a)[i];
        const std::string name = text_of(p, "name");
        const std::string key  = std::string(slot_name) + "/" + std::to_string(i);
        ui::ViewBuilder c = b.panel(3);
        c.label(name);
        c.wrapped(text_of(p, "rationale"), 820, &b.theme().muted, b.theme().small_size);
        ui::ViewBuilder r = c.row(6);
        if (const JsonValue* cfg = p.find("config"); cfg && particle_) {
            const JsonValue config = *cfg;
            r.button("particle \xE3\x81\xB8\xE9\x80\x81\xE3\x82\x8B", [this, config, name] { send_to_particle(config, name); }, true);   // particle へ送る
        }
        const bool open = open_json_.count(key) > 0;
        r.button(open ? "JSON \xE3\x82\x92\xE9\x96\x89\xE3\x81\x98\xE3\x82\x8B" : "JSON", [this, key, open] {   // JSON を閉じる
            if (open) open_json_.erase(key);
            else open_json_.insert(key);
            touch();
        });
        if (open) {
            if (const JsonValue* cfg = p.find("config")) render_json(c, *cfg, {3, 30, 700});
        }
    }
}

} // namespace ergo::studio::views
