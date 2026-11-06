#include "studio_view.h"

#include "json_render.h"

#include <algorithm>
#include <cctype>

namespace ergo::studio::views {
namespace jm = ergo::common::jsonm;

namespace {

JsonValue jstr(const std::string& s) { return JsonValue::make_string(s); }

const JsonArray* array_of(const Slot& s, const char* key) {
    const JsonValue* v = s.result.json.find(key);
    return v && v->is_array() ? v->a.get() : nullptr;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool matches(const std::string& filter, const std::string& text) {
    return filter.empty() || lower(text).find(lower(filter)) != std::string::npos;
}

const ui_kit::Color* state_color(const ui::Theme& t, const std::string& state) {
    if (state == "ok") return &t.ok;
    if (state == "not_configured") return &t.muted;
    if (state == "unauthorized" || state == "degraded") return &t.warn;
    return &t.error;
}

} // namespace

StudioView::StudioView(host::IPlugin& plugin) : ExplorerView(plugin) {}

void StudioView::on_first_show() {
    get("projects", "/api/projects");
    get("templates", "/api/templates");
}

void StudioView::render(ui::ViewBuilder& b) {
    b.title(plugin_.info().title);
    b.muted("LUDIARS \xE3\x81\xAE\xE3\x82\xB5\xE3\x83\xBC\xE3\x83\x93\xE3\x82\xB9\xE3\x81\xA8\xE9\x80\xA3\xE6\x90\xBA\xE3\x81\x97\xE3\x81\xA6\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x82\x92 AI \xE3\x81\xA8\xE4\xBD\x9C\xE3\x82\x8B");   // LUDIARS のサービスと連携してゲームを AI と作る
    render_pages(b);
    render_notice(b);
    switch (page_) {
    case Page::Projects: render_projects(b); break;
    case Page::Tasks: render_tasks(b); break;
    case Page::Runs: render_runs(b); break;
    case Page::Connections: render_connections(b); break;
    case Page::Api: render_explorer(b); break;
    }
}

void StudioView::render_pages(ui::ViewBuilder& b) {
    ui::ViewBuilder r = b.row(6);
    auto tab = [&](Page p, const char* title, const char* slot_name, const char* path) {
        r.button(title, [this, p, slot_name, path] {
            page_ = p;
            if (slot_name) get(slot_name, path);
            touch();
        }, page_ == p);
    };
    tab(Page::Projects, "\xE3\x83\x97\xE3\x83\xAD\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xAF\xE3\x83\x88", "projects", "/api/projects");   // プロジェクト
    tab(Page::Tasks, "\xE3\x82\xBF\xE3\x82\xB9\xE3\x82\xAF", "tasks", "/api/tasks");                                                  // タスク
    tab(Page::Runs, "\xE5\xAE\x9F\xE8\xA1\x8C", "runs", "/api/runs");                                                                 // 実行
    tab(Page::Connections, "\xE6\x8E\xA5\xE7\xB6\x9A", "connections", "/api/connections");                                           // 接続
    tab(Page::Api, "API", nullptr, nullptr);
}

void StudioView::render_projects(ui::ViewBuilder& b) {
    {
        ui::ViewBuilder r = b.row(6);
        r.text_field(filter_, "\xE7\xB5\x9E\xE3\x82\x8A\xE8\xBE\xBC\xE3\x81\xBF (\xE3\x82\xB3\xE3\x83\xBC\xE3\x83\x89 / \xE5\x90\x8D\xE5\x89\x8D)", 280);   // 絞り込み (コード / 名前)
        r.button("\xE6\x9B\xB4\xE6\x96\xB0", [this] { get("projects", "/api/projects"); });                                                                  // 更新
    }
    ui::ViewBuilder list = b.panel(3);
    if (!slot_state(list, "projects")) return;
    const JsonArray* projects = array_of(slot("projects"), "projects");
    if (!projects || projects->empty()) {
        list.muted("\xE7\x99\xBB\xE9\x8C\xB2\xE3\x81\x95\xE3\x82\x8C\xE3\x81\x9F\xE3\x83\x97\xE3\x83\xAD\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xAF\xE3\x83\x88\xE3\x81\x8C\xE3\x81\x82\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93");   // 登録されたプロジェクトがありません
        return;
    }
    const JsonValue* selected = nullptr;
    std::size_t shown = 0;
    for (const auto& p : *projects) {
        const std::string code = text_of(p, "code");
        if (code == project_) selected = &p;
        if (!matches(filter_, code + " " + text_of(p, "project"))) continue;
        if (++shown > 60) continue;
        list.list_item(code + "  " + text_of(p, "project"), code == project_, [this, code] { select_project(code); },
                       text_of(p, "repo_path"));
    }
    if (shown > 60) list.muted("\xE2\x80\xA6 " + std::to_string(shown - 60) + " more");
    if (selected) render_project_detail(b, *selected);
}

void StudioView::select_project(const std::string& code) {
    project_ = code;
    get("source", "/api/sources/" + url_encode(code));
    touch();
}

void StudioView::render_project_detail(ui::ViewBuilder& b, const JsonValue& project) {
    ui::ViewBuilder d = b.panel(5);
    d.label(text_of(project, "code") + "  " + text_of(project, "project"), nullptr, b.theme().title_size);
    d.field("repo", text_of(project, "repo_path"));
    d.field("origin", text_of(project, "repo_origin"));

    ui::ViewBuilder src = d.panel(4);
    src.muted("\xE3\x82\xBD\xE3\x83\xBC\xE3\x82\xB9");   // ソース
    if (slot_state(src, "source")) {
        const JsonValue& s = slot("source").result.json;
        src.field("repository", text_of(s, "repository"));
        if (const JsonValue* st = s.find("status"); st && st->is_object()) render_json(src, *st, {2, 20, 760});
        const std::string st_err = text_of(s, "statusError");
        if (!st_err.empty()) src.label(st_err, &b.theme().warn);
        if (const JsonValue* prs = s.find("prs"); prs && prs->is_array()) {
            src.field("open PR", std::to_string(prs->a->size()));
            for (std::size_t i = 0; i < prs->a->size() && i < 10; ++i) {
                const JsonValue& pr = (*prs->a)[i];
                src.muted("#" + text_of(pr, "number") + "  " + text_of(pr, "title"));
            }
        }
        if (const JsonValue* ff = s.find("fastForward"); ff && ff->is_object()) {
            const bool allowed = ff->find("allowed") && ff->find("allowed")->as_bool();
            src.field("fast-forward", allowed ? "\xE5\x8F\xAF" : text_of(*ff, "reason"));   // 可
        }
    }
    {
        ui::ViewBuilder r = src.row(6);
        const std::string code = text_of(project, "code");
        r.button("fetch", [this, code] {
            post("/api/sources/" + url_encode(code) + "/fetch", JsonValue::make_object(), "fetch \xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F", "source",   // しました
                 "/api/sources/" + url_encode(code));
        });
        r.button("fast-forward", [this, code] {
            post("/api/sources/" + url_encode(code) + "/fast-forward", JsonValue::make_object(),
                 "fast-forward \xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F", "source", "/api/sources/" + url_encode(code));
        });
    }

    ui::ViewBuilder ins = d.panel(5);
    ins.muted("AI \xE3\x81\xB8\xE3\x81\xAE\xE6\x8C\x87\xE7\xA4\xBA (Concordia \xE3\x81\xAE\xE5\xA7\x94\xE8\xAD\xB2)");   // AI への指示 (Concordia の委譲)
    render_templates(ins);
    ins.text_area(instruction_, 760, 5);
    ins.button("\xE6\x8C\x87\xE7\xA4\xBA\xE3\x82\x92\xE9\x80\x81\xE3\x82\x8B", [this] { instruct(); }, true);   // 指示を送る
}

void StudioView::render_templates(ui::ViewBuilder& b) {
    const Slot& s = slot("templates");
    if (!s.loaded) {
        slot_state(b, "templates");
        return;
    }
    if (!s.result.ok()) {
        b.label(s.result.error_text(), &b.theme().error);
        return;
    }
    const JsonArray* list = array_of(s, "templates");
    if (!list) return;
    std::vector<std::string> names;
    for (const auto& t : *list) {
        const JsonValue* active = t.find("is_active");
        if (active && active->is_number() && active->n == 0) continue;
        names.push_back(text_of(t, "call_name"));
    }
    if (template_.empty() && !names.empty()) template_ = names.front();
    for (std::size_t i = 0; i < names.size(); i += 6) {
        ui::ViewBuilder r = b.row(6);
        for (std::size_t j = i; j < std::min(names.size(), i + 6); ++j) {
            const std::string n = names[j];
            r.button(n, [this, n] {
                template_ = n;
                touch();
            }, n == template_);
        }
    }
}

void StudioView::post(const std::string& sub, const JsonValue& body, const std::string& done_text,
                      const std::string& refresh_slot, const std::string& refresh_path) {
    fetch("action", "POST", sub, jm::serialize(body), [this, done_text, refresh_slot, refresh_path](const ApiResult& r) {
        if (r.ok()) {
            std::string text = done_text;
            if (const JsonValue* run = r.json.find("run")) {
                const std::string id = text_of(*run, "id").empty() ? text_of(*run, "run_id") : text_of(*run, "id");
                if (!id.empty()) text += "  run " + id;
            }
            set_notice(text, true);
            if (!refresh_slot.empty()) get(refresh_slot, refresh_path);
        } else {
            set_notice(r.error_text(), false);
        }
    });
}

void StudioView::instruct() {
    if (project_.empty() || template_.empty() || instruction_.empty()) {
        set_notice("\xE3\x83\x97\xE3\x83\xAD\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xAF\xE3\x83\x88\xE3\x83\xBB\xE3\x83\x86\xE3\x83\xB3\xE3\x83\x97\xE3\x83\xAC\xE3\x83\xBC\xE3\x83\x88\xE3\x83\xBB\xE6\x8C\x87\xE7\xA4\xBA\xE6\x96\x87\xE3\x81\x8C\xE8\xA6\x81\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x99", false);   // プロジェクト・テンプレート・指示文が要ります
        return;
    }
    JsonValue body = JsonValue::make_object();
    body.set("call_name", jstr(template_));
    JsonValue args = JsonValue::make_object();
    args.set("task", jstr(instruction_));
    body.set("args", args);
    post("/api/projects/" + url_encode(project_) + "/instruct", body,
         "\xE6\x8C\x87\xE7\xA4\xBA\xE3\x82\x92\xE9\x80\x81\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F", "runs", "/api/runs");   // 指示を送りました
    instruction_.clear();
}

void StudioView::render_tasks(ui::ViewBuilder& b) {
    {
        ui::ViewBuilder r = b.row(6);
        r.text_field(filter_, "\xE7\xB5\x9E\xE3\x82\x8A\xE8\xBE\xBC\xE3\x81\xBF", 280);   // 絞り込み
        r.button("\xE6\x9B\xB4\xE6\x96\xB0", [this] { get("tasks", "/api/tasks"); });   // 更新
    }
    {
        ui::ViewBuilder f = b.panel(5);
        f.muted("\xE6\x96\xB0\xE3\x81\x97\xE3\x81\x84\xE3\x82\xBF\xE3\x82\xB9\xE3\x82\xAF (Actio)");   // 新しいタスク (Actio)
        f.text_field(new_title_, "\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\x88\xE3\x83\xAB", 600);           // タイトル
        f.text_area(new_description_, 760, 3);
        f.button("\xE7\x99\xBB\xE9\x8C\xB2", [this] { create_task(); }, true);                        // 登録
    }
    ui::ViewBuilder list = b.panel(3);
    if (!slot_state(list, "tasks")) return;
    const JsonArray* tasks = array_of(slot("tasks"), "tasks");
    if (!tasks || tasks->empty()) {
        list.muted("\xE3\x82\xBF\xE3\x82\xB9\xE3\x82\xAF\xE3\x81\x8C\xE3\x81\x82\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93");   // タスクがありません
        return;
    }
    const JsonValue* selected = nullptr;
    std::size_t shown = 0;
    for (const auto& t : *tasks) {
        const std::string id = text_of(t, "id");
        if (id == task_id_) selected = &t;
        if (!matches(filter_, text_of(t, "title") + " " + text_of(t, "category"))) continue;
        if (++shown > 80) continue;
        list.list_item(ui::fit_text(text_of(t, "title"), 760, b.theme().font_size, b.measure()), id == task_id_,
                       [this, id] {
                           task_id_ = id;
                           touch();
                       },
                       text_of(t, "status"));
    }
    if (!selected) return;
    ui::ViewBuilder d = b.panel(5);
    d.label(text_of(*selected, "title"), nullptr, b.theme().title_size);
    d.field("status", text_of(*selected, "status"));
    d.field("category", text_of(*selected, "category"));
    const std::string desc = text_of(*selected, "description");
    d.paragraph(desc.size() > 3000 ? desc.substr(0, 3000) + "\xE2\x80\xA6" : desc);
    d.muted("\xE5\xAE\x9F\xE8\xA3\x85\xE3\x81\x99\xE3\x82\x8B\xE3\x83\x97\xE3\x83\xAD\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xAF\xE3\x83\x88: " +
            (project_.empty() ? std::string("(\xE3\x83\x97\xE3\x83\xAD\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xAF\xE3\x83\x88\xE7\x94\xBB\xE9\x9D\xA2\xE3\x81\xA7\xE9\x81\xB8\xE3\x81\xB6)") : project_));   // 実装するプロジェクト: / (プロジェクト画面で選ぶ)
    render_templates(d);
    const std::string id = task_id_;
    d.button("AI \xE3\x81\xAB\xE5\xAE\x9F\xE8\xA3\x85\xE3\x81\x95\xE3\x81\x9B\xE3\x82\x8B", [this, id] { implement_task(id); }, true);   // AI に実装させる
}

void StudioView::create_task() {
    if (new_title_.empty()) {
        set_notice("\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\x88\xE3\x83\xAB\xE3\x81\x8C\xE8\xA6\x81\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x99", false);   // タイトルが要ります
        return;
    }
    JsonValue body = JsonValue::make_object();
    body.set("title", jstr(new_title_));
    if (!new_description_.empty()) body.set("description", jstr(new_description_));
    post("/api/tasks", body, "\xE3\x82\xBF\xE3\x82\xB9\xE3\x82\xAF\xE3\x82\x92\xE7\x99\xBB\xE9\x8C\xB2\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F", "tasks", "/api/tasks");   // タスクを登録しました
    new_title_.clear();
    new_description_.clear();
}

void StudioView::implement_task(const std::string& task_id) {
    if (project_.empty() || template_.empty()) {
        set_notice("\xE3\x83\x97\xE3\x83\xAD\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xAF\xE3\x83\x88\xE3\x81\xA8\xE3\x83\x86\xE3\x83\xB3\xE3\x83\x97\xE3\x83\xAC\xE3\x83\xBC\xE3\x83\x88\xE3\x82\x92\xE9\x81\xB8\xE3\x82\x93\xE3\x81\xA7\xE3\x81\x8F\xE3\x81\xA0\xE3\x81\x95\xE3\x81\x84", false);   // プロジェクトとテンプレートを選んでください
        return;
    }
    JsonValue body = JsonValue::make_object();
    body.set("code", jstr(project_));
    body.set("call_name", jstr(template_));
    post("/api/tasks/" + url_encode(task_id) + "/implement", body,
         "\xE5\xAE\x9F\xE8\xA3\x85\xE3\x82\x92\xE4\xBE\x9D\xE9\xA0\xBC\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F", "runs", "/api/runs");   // 実装を依頼しました
}

void StudioView::render_runs(ui::ViewBuilder& b) {
    b.button("\xE6\x9B\xB4\xE6\x96\xB0", [this] { get("runs", "/api/runs"); });   // 更新
    ui::ViewBuilder list = b.panel(3);
    if (!slot_state(list, "runs")) return;
    const JsonArray* runs = array_of(slot("runs"), "runs");
    if (!runs || runs->empty()) {
        list.muted("\xE3\x81\x93\xE3\x81\xAE Studio \xE3\x81\x8B\xE3\x82\x89\xE3\x81\xAE\xE5\xAE\x9F\xE8\xA1\x8C\xE3\x81\xAF\xE3\x81\x82\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93");   // この Studio からの実行はありません
        return;
    }
    for (const auto& r : *runs) {
        list.list_item(text_of(r, "call_name") + "  " + text_of(r, "id"), false, {}, text_of(r, "status"));
    }
}

void StudioView::render_connections(ui::ViewBuilder& b) {
    b.button("\xE5\x86\x8D\xE7\xA2\xBA\xE8\xAA\x8D", [this] { get("connections", "/api/connections"); });   // 再確認
    ui::ViewBuilder list = b.panel(4);
    if (!slot_state(list, "connections")) return;
    const JsonValue* nodes = slot("connections").result.json.find("nodes");
    if (!nodes || !nodes->is_array()) {
        render_json(list, slot("connections").result.json);
        return;
    }
    for (const auto& n : *nodes->a) {
        const std::string state = text_of(n, "state");
        ui::ViewBuilder r = list.row(10);
        r.label(text_of(n, "label").empty() ? text_of(n, "id") : text_of(n, "label"));
        r.label(state, state_color(b.theme(), state));
        const std::string detail = text_of(n, "detail").empty() ? text_of(n, "url") : text_of(n, "detail");
        if (!detail.empty()) r.muted(detail);
    }
}

} // namespace ergo::studio::views
