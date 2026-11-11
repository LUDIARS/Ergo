#include "git_view.h"

#include "json_render.h"

namespace ergo::studio::views {
namespace jm = ergo::common::jsonm;

namespace {

std::string number_of(const jm::JsonValue& v, const char* key) {
    const jm::JsonValue* f = v.find(key);
    return f != nullptr && f->is_number() ? std::to_string(static_cast<long long>(f->as_number())) : "0";
}

} // namespace

void GitView::on_first_show() {
    get("repos", "/api/repos");
    if (repo_ >= 0) refresh();
}

void GitView::select(int index) {
    repo_ = index;
    message_.clear();
    new_branch_.clear();
    refresh();
    touch();
}

void GitView::refresh() {
    const std::string base = "/api/repos/" + std::to_string(repo_);
    get("status", base + "/status");
    get("branches", base + "/branches");
    get("log", base + "/log");
}

void GitView::post(const std::string& what, const std::string& body, const std::string& done) {
    fetch("action", "POST", "/api/repos/" + std::to_string(repo_) + "/" + what, body, [this, done](const ApiResult& r) {
        const std::string said = text_of(r.json, "message");
        set_notice(r.ok() ? done + (said.empty() ? "" : "\n" + said) : r.error_text(), r.ok());
        refresh();
    });
}

void GitView::render(ui::ViewBuilder& b) {
    b.title(plugin_.info().title);
    b.muted(plugin_.info().description);
    render_notice(b);
    if (!slot_state(b, "repos")) return;
    const auto& repos = slot("repos").result.json;
    if (!repos.is_array() || repos.a->empty()) {
        b.paragraph("扱うリポジトリがありません。ワークスペースのプロフィール (git) にリポジトリを書いてください。");
        return;
    }
    {
        ui::ViewBuilder tabs = b.row(4);
        int i = 0;
        for (const auto& r : *repos.a) {
            const std::string profile = text_of(r, "profile");
            tabs.button(profile + ": " + text_of(r, "path"), [this, i] { select(i); }, i == repo_);
            ++i;
        }
    }
    if (repo_ < 0) return;
    // State: branch, distance from the upstream, changed files.
    if (slot_state(b, "status")) {
        const auto& st = slot("status").result.json;
        const std::string upstream = text_of(st, "upstream");
        b.field("ブランチ", text_of(st, "branch") + (upstream.empty() ? "  (上流なし)" : "  ← " + upstream + "  先行 " +
                                                                                   number_of(st, "ahead") + " / 遅れ " +
                                                                                   number_of(st, "behind")));
        const jm::JsonValue* files = st.find("files");
        if (files != nullptr && files->is_array() && !files->a->empty()) {
            ui::ViewBuilder panel = b.panel(2);
            for (const auto& f : *files->a) panel.label(text_of(f, "state") + "  " + text_of(f, "path"));
        } else {
            b.muted("変更はありません");
        }
    }
    {
        ui::ViewBuilder r = b.row(6);
        r.button("取得 (fetch)", [this] { post("fetch", "{}", "取得しました"); });
        r.button("pull", [this] { post("pull", "{}", "pull しました"); });
        r.button("push", [this] { post("push", "{}", "push しました"); });
        r.button("更新", [this] { refresh(); });
    }
    // Commit: stages every change.
    b.label("コミット (変更をすべて含めます)");
    b.text_area(message_, 640, 3);
    b.button("コミット", [this] {
        jm::JsonValue body = jm::JsonValue::make_object();
        body.set("message", jm::JsonValue::make_string(message_));
        post("commit", jm::serialize(body), "コミットしました");
        message_.clear();
    }, true);
    // Branches.
    b.label("ブランチ");
    if (slot_state(b, "branches")) {
        const auto& list = slot("branches").result.json;
        ui::ViewBuilder panel = b.panel(2);
        if (list.is_array()) {
            for (const auto& br : *list.a) {
                const std::string name = text_of(br, "name");
                const bool current = br.find("current") && br.find("current")->as_bool();
                ui::ViewBuilder r = panel.row(8);
                r.clipped_label(name, 360, current ? &b.theme().ok : nullptr);
                if (!current) {
                    r.button("切り替え", [this, name] {
                        jm::JsonValue body = jm::JsonValue::make_object();
                        body.set("branch", jm::JsonValue::make_string(name));
                        post("switch", jm::serialize(body), name + " に切り替えました");
                    });
                }
            }
        }
    }
    {
        ui::ViewBuilder r = b.row(6);
        r.text_field(new_branch_, "新しいブランチ (feat/...)", 300);
        r.button("作成して切り替え", [this] {
            jm::JsonValue body = jm::JsonValue::make_object();
            body.set("branch", jm::JsonValue::make_string(new_branch_));
            body.set("create", jm::JsonValue::make_bool(true));
            post("switch", jm::serialize(body), new_branch_ + " を作りました");
            new_branch_.clear();
        });
    }
    // History.
    b.label("最近のコミット");
    if (slot_state(b, "log")) {
        const auto& log = slot("log").result.json;
        if (log.is_array()) {
            ui::ViewBuilder panel = b.panel(2);
            for (const auto& c : *log.a) {
                panel.clipped_label(text_of(c, "sha") + "  " + text_of(c, "subject") + "  (" + text_of(c, "author") + ")", 760);
            }
        }
    }
}

} // namespace ergo::studio::views
