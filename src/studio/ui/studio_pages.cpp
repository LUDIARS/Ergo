#include "ergo/studio/ui/studio_pages.h"

#include "ergo/common/json_min.h"
#include "ergo/studio/host/health_report.h"

#include <cstdio>

namespace ergo::studio::ui {
namespace jm = ergo::common::jsonm;

namespace {

std::string scalar_text(const jm::JsonValue& v) {
    if (v.is_string()) return v.s;
    if (v.is_bool()) return v.b ? "true" : "false";
    if (v.is_number()) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%g", v.n);
        return buf;
    }
    return jm::serialize(v);
}

bool health_ok(const jm::JsonValue& h) {
    const jm::JsonValue* ok = h.find("ok");
    return ok && ok->as_bool();
}

} // namespace

const std::string& overview_title() {
    static const std::string t = "\xE6\xA6\x82\xE8\xA6\x81";   // 概要
    return t;
}

std::size_t game_scene_tab(const host::PluginHost& host) {
    for (std::size_t i = 0; i < host.plugins().size(); ++i)
        if (host.plugins()[i]->info().id == "play") return i + 1;
    return 0;
}

host::IPluginView* view_of(host::IPlugin& p, const ViewLookup& views) {
    if (host::IPluginView* v = p.view()) return v;
    return views ? views(p) : nullptr;
}

void build_tab_list(ViewBuilder& bar, const host::PluginHost& host, std::size_t active,
                    const std::function<void(std::size_t)>& select) {
    auto tab = [&](std::size_t i, const std::string& title) {
        bar.list_item(title, i == active, [select, i] { select(i); });
    };
    tab(0, overview_title());
    for (std::size_t i = 0; i < host.plugins().size(); ++i) tab(i + 1, host.plugins()[i]->info().title);
}

void build_overview(ViewBuilder& b, const host::PluginHost& host) {
    b.title("Ergo Studio");
    b.muted("\xE8\xAA\xAD\xE3\x81\xBF\xE8\xBE\xBC\xE3\x82\x93\xE3\x81\xA0\xE3\x83\x97\xE3\x83\xA9\xE3\x82\xB0\xE3\x82\xA4\xE3\x83\xB3\xE3\x81\xA8\xE7\x8A\xB6\xE6\x85\x8B");   // 読み込んだプラグインと状態
    for (host::IPlugin* p : host.plugins()) {
        const auto h = host::plugin_health(*p);
        ViewBuilder panel = b.panel(4);
        panel.status(health_ok(h), p->info().title + "  (" + p->info().id + ")");
        if (const jm::JsonValue* err = h.find("err")) panel.muted(scalar_text(*err));
        else if (const jm::JsonValue* w = h.find("warning")) panel.muted(scalar_text(*w));
    }
    for (const auto& r : host.rejected()) {
        ViewBuilder panel = b.panel(4);
        panel.status(false, "\xE8\xAA\xAD\xE3\x81\xBF\xE8\xBE\xBC\xE3\x82\x81\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xA3\xE3\x81\x9F\xE5\xA4\x96\xE9\x83\xA8\xE3\x83\x91\xE3\x83\x83\xE3\x82\xAF: " + r.source);   // 読み込めなかった外部パック
        panel.muted(r.reason);
    }
}

void build_generic(ViewBuilder& b, host::IPlugin& p) {
    const auto& info = p.info();
    b.title(info.title);
    if (!info.description.empty()) b.muted(info.description);
    const auto h = host::plugin_health(p);
    ViewBuilder panel = b.panel(4);
    panel.status(health_ok(h), health_ok(h) ? "ok" : "not ok");
    for (const auto& [k, v] : *h.o) {
        if (k == "ok" || k == "id" || k == "title") continue;
        panel.field(k, scalar_text(v));
    }
    ViewBuilder ep = b.panel(4);
    ep.muted("\xE3\x82\xA8\xE3\x83\xB3\xE3\x83\x89\xE3\x83\x9D\xE3\x82\xA4\xE3\x83\xB3\xE3\x83\x88");   // エンドポイント
    if (p.http()) ep.label("HTTP  /" + info.id + "/api/...");
    if (p.socket()) ep.label("WS    /" + info.id + "/ws");
}

} // namespace ergo::studio::ui
