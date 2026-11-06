#include "explorer_view.h"

#include "json_render.h"

#include <algorithm>
#include <map>

namespace ergo::studio::views {

std::vector<std::string> known_get_endpoints(const std::string& id) {
    static const std::map<std::string, std::vector<std::string>> table = {
        {"studio", {"/api/projects", "/api/tasks", "/api/runs", "/api/templates", "/api/sources", "/api/plugins",
                    "/api/connections", "/api/endpoints", "/api/status"}},
        {"particle", {"/api/effect"}},
        {"variable", {"/api/vars", "/api/actors"}},
        {"rive", {"/api/meta"}},
        {"profile", {"/api/snapshot"}},
        {"pictor_perf", {"/api/snapshot"}},
        {"render_pipeline", {"/api/profiles"}},
        {"visus", {"/api/roots", "/api/scan"}},
        {"ui_layout", {"/api/bridge/config"}},
        {"vfx-suggest", {"/api/games", "/api/catalog"}},
        {"vfx-breed", {"/api/seeds", "/api/genes"}},
        {"scene_editor", {"/api/scenes"}},
        {"log", {"/api/apps"}},
        {"musa_melpomene", {"/api/config", "/api/issues", "/api/notifications", "/api/pulls", "/api/runs",
                            "/api/milestones", "/api/cache-stats", "/api/user"}},
        {"musa_terpsichore", {"/api/status", "/api/config"}},
        {"musa_thaleia", {"/api/tree", "/api/categories"}},
    };
    std::vector<std::string> out{"/api/health"};
    if (const auto it = table.find(id); it != table.end()) out.insert(out.end(), it->second.begin(), it->second.end());
    return out;
}

ExplorerView::ExplorerView(host::IPlugin& plugin) : ApiView(plugin) {}

void ExplorerView::on_first_show() { get("health", "/api/health"); }

void ExplorerView::send() {
    std::string m = method_;
    std::transform(m.begin(), m.end(), m.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (m.empty()) m = "GET";
    std::string p = path_.empty() || path_[0] != '/' ? "/" + path_ : path_;
    last_label_   = m + " /" + plugin_.info().id + p;
    fetch("explore", m, p, m == "GET" ? std::string() : body_);
}

void ExplorerView::render(ui::ViewBuilder& b) {
    const auto& info = plugin_.info();
    b.title(info.title);
    if (!info.description.empty()) b.muted(info.description);
    {
        ui::ViewBuilder h = b.panel(4);
        const Slot& s = slot("health");
        if (s.loaded) {
            h.status(s.result.ok() && s.result.json.find("ok") && s.result.json.find("ok")->as_bool(),
                     "health  (" + std::to_string(s.result.status) + ")");
            if (s.result.json.is_object()) {
                for (const auto& [k, v] : *s.result.json.o) {
                    if (k != "ok" && k != "id" && k != "title") h.field(k, scalar_text(v));
                }
            }
        } else {
            slot_state(h, "health");
        }
        h.button("\xE6\x9B\xB4\xE6\x96\xB0", [this] { get("health", "/api/health"); });   // 更新
    }
    render_explorer(b);
}

void ExplorerView::render_explorer(ui::ViewBuilder& b) {
    ui::ViewBuilder p = b.panel(6);
    p.muted("API");
    const auto eps = known_get_endpoints(plugin_.info().id);
    for (std::size_t i = 0; i < eps.size(); i += 5) {   // rows do not wrap: 5 buttons a row
        ui::ViewBuilder r = p.row(6);
        for (std::size_t j = i; j < std::min(eps.size(), i + 5); ++j) {
            const std::string ep = eps[j];
            r.button(ep, [this, ep] {
                method_ = "GET";
                path_   = ep;
                send();
            });
        }
    }
    {
        ui::ViewBuilder r = p.row(6);
        r.text_field(method_, "GET", 90, [this] { send(); });
        r.text_field(path_, "/api/...", 520, [this] { send(); });
        r.button("\xE9\x80\x81\xE4\xBF\xA1", [this] { send(); }, true);   // 送信
    }
    p.muted("body (JSON, GET \xE4\xBB\xA5\xE5\xA4\x96)");   // GET 以外
    p.text_area(body_, 720, 4);

    const Slot& s = slot("explore");
    if (!s.loaded && !s.loading) return;
    ui::ViewBuilder res = b.panel(4);
    res.muted(last_label_);
    if (s.loading) {
        res.muted("\xE8\xAA\xAD\xE3\x81\xBF\xE8\xBE\xBC\xE3\x81\xBF\xE4\xB8\xAD\xE2\x80\xA6");   // 読み込み中…
        return;
    }
    res.status(s.result.ok(), "HTTP " + std::to_string(s.result.status));
    if (!s.result.json.is_null() || s.result.body == "null") render_json(res, s.result.json);
    else res.paragraph(s.result.body.substr(0, 4000));
}

} // namespace ergo::studio::views
