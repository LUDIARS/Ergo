#include "musa_views.h"

#include "json_render.h"

#include <algorithm>
#include <cctype>

namespace ergo::studio::views {
namespace jm = ergo::common::jsonm;

namespace {

JsonValue jstr(const std::string& s) { return JsonValue::make_string(s); }

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

/// Pretty JSON for editing (2-space indent; the minimal serializer is compact).
void pretty(const JsonValue& v, std::string& out, int indent) {
    const std::string pad(static_cast<std::size_t>(indent) * 2, ' ');
    if (v.is_object() && !v.o->empty()) {
        out += "{\n";
        std::size_t i = 0;
        for (const auto& [k, c] : *v.o) {
            out += pad + "  " + jm::serialize(JsonValue::make_string(k)) + ": ";
            pretty(c, out, indent + 1);
            out += ++i < v.o->size() ? ",\n" : "\n";
        }
        out += pad + "}";
    } else if (v.is_array() && !v.a->empty()) {
        out += "[\n";
        for (std::size_t i = 0; i < v.a->size(); ++i) {
            out += pad + "  ";
            pretty((*v.a)[i], out, indent + 1);
            out += i + 1 < v.a->size() ? ",\n" : "\n";
        }
        out += pad + "]";
    } else {
        out += jm::serialize(v);
    }
}

std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= s.size()) {
        const auto c = s.find(',', start);
        std::string part = s.substr(start, c == std::string::npos ? std::string::npos : c - start);
        part.erase(0, part.find_first_not_of(' '));
        part.erase(part.find_last_not_of(' ') + 1);
        if (!part.empty()) out.push_back(part);
        if (c == std::string::npos) break;
        start = c + 1;
    }
    return out;
}

} // namespace

// ── Terpsichore ─────────────────────────────────────────────────────────────

TerpsichoreView::TerpsichoreView(host::IPlugin& plugin) : ExplorerView(plugin) {}

void TerpsichoreView::load_config_text(const ApiResult& r) {
    const JsonValue* c = r.json.find("config");
    if (c && !c->is_null()) {
        config_text_.clear();
        pretty(*c, config_text_, 0);
    }
}

void TerpsichoreView::on_first_show() {
    get("status", "/api/status");
    fetch("config", "GET", "/api/config", {}, [this](const ApiResult& r) { load_config_text(r); });
}

void TerpsichoreView::render(ui::ViewBuilder& b) {
    b.title(plugin_.info().title);
    b.muted(plugin_.info().description);
    render_notice(b);
    ui::ViewBuilder st = b.panel(4);
    if (slot_state(st, "status")) {
        const JsonValue& s = slot("status").result.json;
        const bool running = s.find("running") && s.find("running")->as_bool();
        st.status(running, running ? "\xE7\xA8\xBC\xE5\x83\x8D\xE4\xB8\xAD" : "\xE5\x81\x9C\xE6\xAD\xA2\xE4\xB8\xAD");   // 稼働中 / 停止中
        for (const char* k : {"port", "role", "uptime_seconds", "request_count"}) st.field(k, text_of(s, k));
    }
    {
        ui::ViewBuilder r = st.row(6);
        auto act = [this](const char* path, const char* ok) {
            fetch("action", "POST", path, "{}", [this, ok](const ApiResult& res) {
                set_notice(res.ok() ? ok : res.error_text(), res.ok());
                get("status", "/api/status");
            });
        };
        r.button("\xE8\xB5\xB7\xE5\x8B\x95", [act] { act("/api/start", "\xE8\xB5\xB7\xE5\x8B\x95\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F"); }, true);   // 起動 / 起動しました
        r.button("\xE5\x81\x9C\xE6\xAD\xA2", [act] { act("/api/stop", "\xE5\x81\x9C\xE6\xAD\xA2\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F"); });            // 停止 / 停止しました
        r.button("\xE6\x9B\xB4\xE6\x96\xB0", [this] { get("status", "/api/status"); });                                                                       // 更新
    }
    ui::ViewBuilder cfg = b.panel(4);
    cfg.muted("\xE8\xA8\xAD\xE5\xAE\x9A (config.json)");   // 設定
    const Slot& c = slot("config");
    if (c.loaded) {
        if (const std::string p = text_of(c.result.json, "path"); !p.empty()) cfg.muted(p);
        if (const std::string e = text_of(c.result.json, "error"); !e.empty()) cfg.label(e, &b.theme().warn);
    }
    cfg.text_area(config_text_, 760, 12);
    cfg.button("\xE4\xBF\x9D\xE5\xAD\x98", [this] {   // 保存
        fetch("action", "POST", "/api/config", config_text_, [this](const ApiResult& r) {
            set_notice(r.ok() ? "\xE4\xBF\x9D\xE5\xAD\x98\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F" : r.error_text(), r.ok());   // 保存しました
            if (r.ok()) load_config_text(r);
        });
    }, true);
}

// ── Melpomene ───────────────────────────────────────────────────────────────

MelpomeneView::MelpomeneView(host::IPlugin& plugin) : ExplorerView(plugin) {}

void MelpomeneView::on_first_show() {
    fetch("config", "GET", "/api/config", {}, [this](const ApiResult& r) {
        if (!r.ok() || settings_loaded_) return;
        settings_loaded_ = true;
        owner_ = text_of(r.json, "repository_owner");
        repo_  = text_of(r.json, "repository_name");
        const bool configured = r.json.find("is_configured") && r.json.find("is_configured")->as_bool();
        if (configured) get("issues", "/api/issues");
        else page_ = Page::Settings;
    });
}

void MelpomeneView::post_and_notice(const std::string& sub, const JsonValue& body, const std::string& ok_text,
                                    std::function<void()> after) {
    fetch("action", "POST", sub, jm::serialize(body), [this, ok_text, after](const ApiResult& r) {
        set_notice(r.ok() ? ok_text : r.error_text(), r.ok());
        if (r.ok() && after) after();
    });
}

void MelpomeneView::render(ui::ViewBuilder& b) {
    b.title(plugin_.info().title);
    b.muted(plugin_.info().description);
    {
        ui::ViewBuilder r = b.row(6);
        auto tab = [&](Page p, const char* title) {
            r.button(title, [this, p] {
                page_ = p;
                if (p == Page::Notifications) get("notifications", "/api/notifications");
                touch();
            }, page_ == p);
        };
        tab(Page::Issues, "Issue");
        tab(Page::New, "\xE6\x96\xB0\xE8\xA6\x8F");                          // 新規
        tab(Page::Notifications, "\xE9\x80\x9A\xE7\x9F\xA5");                // 通知
        tab(Page::Settings, "\xE8\xA8\xAD\xE5\xAE\x9A");                     // 設定
    }
    render_notice(b);
    switch (page_) {
    case Page::Issues: render_issues(b); break;
    case Page::New: render_new(b); break;
    case Page::Notifications: {
        ui::ViewBuilder p = b.panel(3);
        if (slot_state(p, "notifications")) render_json(p, slot("notifications").result.json);
        break;
    }
    case Page::Settings: render_settings(b); break;
    }
}

void MelpomeneView::render_issues(ui::ViewBuilder& b) {
    {
        ui::ViewBuilder r = b.row(6);
        r.text_field(filter_, "\xE7\xB5\x9E\xE3\x82\x8A\xE8\xBE\xBC\xE3\x81\xBF (\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\x88\xE3\x83\xAB / \xE3\x82\xB7\xE3\x83\xBC\xE3\x83\xB3 / \xE3\x83\xA9\xE3\x83\x99\xE3\x83\xAB)", 360);   // 絞り込み (タイトル / シーン / ラベル)
        r.button("\xE5\x86\x8D\xE5\x8F\x96\xE5\xBE\x97", [this] { get("issues", "/api/issues?refresh=1"); });   // 再取得
    }
    ui::ViewBuilder list = b.panel(3);
    if (!slot_state(list, "issues")) return;
    const JsonValue& arr = slot("issues").result.json;
    if (!arr.is_array() || arr.a->empty()) {
        list.muted("Issue \xE3\x81\x8C\xE3\x81\x82\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93");   // Issue がありません
        return;
    }
    const std::string f = lower(filter_);
    std::size_t shown = 0;
    for (const auto& t : *arr.a) {
        const std::string num   = text_of(t, "github_issue_number");
        std::string labels;
        if (const JsonValue* l = t.find("labels"); l && l->is_array()) {
            for (const auto& x : *l->a) labels += (labels.empty() ? "" : ", ") + x.as_string();
        }
        const std::string hay = lower(text_of(t, "title") + " " + text_of(t, "scene_name") + " " + labels);
        if (!f.empty() && hay.find(f) == std::string::npos) continue;
        if (++shown > 100) continue;
        list.list_item("#" + num + "  " + ui::fit_text(text_of(t, "title"), 640, b.theme().font_size, b.measure()), num == issue_,
                       [this, num] { open_issue(num); }, text_of(t, "state") + (labels.empty() ? "" : "  " + labels));
    }
    if (!issue_.empty()) render_issue(b);
}

void MelpomeneView::open_issue(const std::string& number) {
    issue_ = number;
    comment_.clear();
    get("comments", "/api/comments?issue=" + url_encode(number));
    touch();
}

void MelpomeneView::render_issue(ui::ViewBuilder& b) {
    const JsonValue& arr = slot("issues").result.json;
    const JsonValue* issue = nullptr;
    for (const auto& t : *arr.a) {
        if (text_of(t, "github_issue_number") == issue_) issue = &t;
    }
    if (!issue) return;
    ui::ViewBuilder d = b.panel(5);
    d.label("#" + issue_ + "  " + text_of(*issue, "title"), nullptr, b.theme().title_size);
    for (const char* k : {"state", "priority", "category", "scene_name", "object_path", "author", "updated_at"}) {
        const std::string v = text_of(*issue, k);
        if (!v.empty()) d.field(k, v);
    }
    d.wrapped(text_of(*issue, "description"), 900);
    ui::ViewBuilder cm = d.panel(3);
    cm.muted("\xE3\x82\xB3\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88");   // コメント
    if (slot_state(cm, "comments")) {
        const JsonValue& cs = slot("comments").result.json;
        if (cs.is_array()) {
            for (const auto& c : *cs.a) {
                cm.muted(text_of(c, "user") + "  " + text_of(c, "created_at"));
                cm.wrapped(text_of(c, "body"), 880);
            }
        }
    }
    cm.text_area(comment_, 760, 3);
    ui::ViewBuilder r = cm.row(6);
    const std::string num = issue_;
    r.button("\xE3\x82\xB3\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88\xE3\x81\x99\xE3\x82\x8B", [this, num] {   // コメントする
        if (comment_.empty()) return;
        JsonValue body = JsonValue::make_object();
        body.set("issue", JsonValue::make_number(std::stod(num)));
        body.set("body", jstr(comment_));
        comment_.clear();
        post_and_notice("/api/comments", body, "\xE3\x82\xB3\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F",   // コメントしました
                        [this, num] { get("comments", "/api/comments?issue=" + url_encode(num)); });
    }, true);
    if (text_of(*issue, "state") != "closed") {
        r.button("\xE9\x96\x89\xE3\x81\x98\xE3\x82\x8B", [this, num] {   // 閉じる
            JsonValue body = JsonValue::make_object();
            body.set("number", JsonValue::make_number(std::stod(num)));
            post_and_notice("/api/issue/close", body, "\xE9\x96\x89\xE3\x81\x98\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F",   // 閉じました
                            [this] { get("issues", "/api/issues?refresh=1"); });
        });
    }
}

void MelpomeneView::render_new(ui::ViewBuilder& b) {
    ui::ViewBuilder f = b.panel(5);
    f.text_field(new_title_, "\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\x88\xE3\x83\xAB", 700);   // タイトル
    f.muted("\xE6\x9C\xAC\xE6\x96\x87");                                                   // 本文
    f.text_area(new_body_, 760, 6);
    {
        ui::ViewBuilder r = f.row(6);
        r.text_field(new_labels_, "labels (a, b)", 240);
        r.text_field(new_scene_, "scene", 200);
        r.text_field(new_object_, "object path", 260);
    }
    f.button("Issue \xE3\x82\x92\xE4\xBD\x9C\xE3\x82\x8B", [this] {   // Issue を作る
        if (new_title_.empty()) {
            set_notice("\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\x88\xE3\x83\xAB\xE3\x81\x8C\xE8\xA6\x81\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x99", false);   // タイトルが要ります
            return;
        }
        JsonValue body = JsonValue::make_object();
        body.set("title", jstr(new_title_));
        body.set("body", jstr(new_body_));
        const auto labels = split_csv(new_labels_);
        if (!labels.empty()) {
            JsonValue l = JsonValue::make_array();
            for (const auto& x : labels) l.push(jstr(x));
            body.set("labels", l);
        }
        if (!new_scene_.empty() || !new_object_.empty()) {
            JsonValue t = JsonValue::make_object();
            if (!new_scene_.empty()) t.set("scene_name", jstr(new_scene_));
            if (!new_object_.empty()) t.set("object_path", jstr(new_object_));
            body.set("ticket", t);
        }
        post_and_notice("/api/issues", body, "Issue \xE3\x82\x92\xE4\xBD\x9C\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F", [this] {   // Issue を作りました
            get("issues", "/api/issues?refresh=1");
        });
        new_title_.clear();
        new_body_.clear();
    }, true);
}

void MelpomeneView::render_settings(ui::ViewBuilder& b) {
    ui::ViewBuilder f = b.panel(5);
    const Slot& c = slot("config");
    if (c.loaded && c.result.ok()) {
        const bool has_token = c.result.json.find("has_token") && c.result.json.find("has_token")->as_bool();
        f.status(has_token, has_token ? "GitHub \xE3\x83\x88\xE3\x83\xBC\xE3\x82\xAF\xE3\x83\xB3: \xE8\xA8\xAD\xE5\xAE\x9A\xE6\xB8\x88\xE3\x81\xBF (OS \xE3\x81\xAE\xE8\xB3\x87\xE6\xA0\xBC\xE6\x83\x85\xE5\xA0\xB1)"   // GitHub トークン: 設定済み (OS の資格情報)
                                      : "GitHub \xE3\x83\x88\xE3\x83\xBC\xE3\x82\xAF\xE3\x83\xB3: \xE6\x9C\xAA\xE8\xA8\xAD\xE5\xAE\x9A");                                                           // 未設定
        if (const std::string e = text_of(c.result.json, "error"); !e.empty()) f.label(e, &b.theme().warn);
    } else {
        slot_state(f, "config");
    }
    {
        ui::ViewBuilder r = f.row(6);
        r.text_field(owner_, "owner", 220);
        r.text_field(repo_, "repository", 300);
    }
    f.secret_field(token_, "GitHub token (\xE5\xA4\x89\xE3\x81\x88\xE3\x82\x8B\xE3\x81\xA8\xE3\x81\x8D\xE3\x81\xA0\xE3\x81\x91)", 528);   // 変えるときだけ
    f.button("\xE4\xBF\x9D\xE5\xAD\x98", [this] {   // 保存
        JsonValue body = JsonValue::make_object();
        body.set("repository_owner", jstr(owner_));
        body.set("repository_name", jstr(repo_));
        if (!token_.empty()) body.set("github_token", jstr(token_));
        token_.clear();   // never kept on screen after saving
        post_and_notice("/api/config", body, "\xE4\xBF\x9D\xE5\xAD\x98\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F", [this] {   // 保存しました
            get("config", "/api/config");
            get("issues", "/api/issues?refresh=1");
        });
    }, true);
}

} // namespace ergo::studio::views
