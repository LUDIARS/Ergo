#include "play_view.h"

#include "json_render.h"

namespace ergo::studio::views {

namespace {

constexpr std::size_t kLogLines = 200;

const JsonValue* find_target(const JsonValue& list, const std::string& name) {
    const JsonValue* targets = list.find("targets");
    if (!targets || !targets->is_array()) return nullptr;
    for (const auto& t : *targets->a) {
        if (text_of(t, "name") == name) return &t;
    }
    return nullptr;
}

} // namespace

std::string play_state_text(const std::string& state) {
    if (state == "building") return "ビルド中";
    if (state == "running") return "実行中";
    if (state == "failed") return "失敗";
    return "停止中";
}

void PlayView::on_first_show() {
    get("health", "/api/health");
    get("targets", "/api/targets");
    if (!target_.empty()) refresh_logs();
}

void PlayView::refresh_logs() {
    if (!target_.empty()) get("logs", "/api/targets/" + url_encode(target_) + "/logs?since=0");
}

void PlayView::action(const std::string& name) {
    const std::string target = target_;
    fetch("action", "POST", "/api/targets/" + url_encode(target) + "/" + name, "{}", [this, name, target](const ApiResult& r) {
        set_notice(r.ok() ? target + ": " + name + " を受け付けました" : r.error_text(), r.ok());
        get("targets", "/api/targets");
        get("logs", "/api/targets/" + url_encode(target) + "/logs?since=0");
    });
}

void PlayView::render(ui::ViewBuilder& b) {
    b.title(plugin_.info().title);
    b.muted("ゲームのビルド・実行と遠隔プレイ (映像はブラウザの Studio /shell/ で見られます)");
    {
        const Slot& h = slot("health");
        if (h.loaded && h.result.json.find("ok") && !h.result.json.find("ok")->as_bool()) {
            b.label(text_of(h.result.json, "err"), &b.theme().warn);
        }
    }
    render_notice(b);
    {
        ui::ViewBuilder r = b.row(6);
        r.button("更新", [this] { on_first_show(); });
    }
    if (!slot_state(b, "targets")) return;
    const JsonValue& list = slot("targets").result.json;
    if (const JsonValue* errs = list.find("errors"); errs && errs->is_array()) {
        for (const auto& e : *errs->a) b.label("プロフィールの play を読めません: " + (e.is_string() ? e.s : std::string()), &b.theme().warn);
    }
    const JsonValue* targets = list.find("targets");
    if (!targets || !targets->is_array() || targets->a->empty()) {
        b.paragraph("遠隔プレイの目標がありません。プロフィール (<studio home>/profiles) の play を確認してください。");
        return;
    }
    {
        ui::ViewBuilder pick = b.panel(2);
        for (const auto& t : *targets->a) {
            const std::string name = text_of(t, "name");
            pick.list_item(text_of(t, "label").empty() ? name : text_of(t, "label"), name == target_,
                           [this, name] {
                               if (target_ != name) watching_ = false;
                               target_ = name;
                               refresh_logs();
                               touch();
                           },
                           play_state_text(text_of(t, "state")));
        }
    }
    if (const JsonValue* t = find_target(list, target_)) render_target(b, *t);
}

void PlayView::render_target(ui::ViewBuilder& b, const JsonValue& t) {
    const std::string state = text_of(t, "state");
    const bool busy = state == "building" || state == "running";
    b.separator();
    {
        ui::ViewBuilder r = b.row(8);
        r.label(text_of(t, "label").empty() ? target_ : text_of(t, "label"), nullptr, b.theme().title_size);
        r.label(play_state_text(state), state == "running" ? &b.theme().ok : state == "failed" ? &b.theme().error : nullptr);
        if (const JsonValue* code = t.find("exit_code"); code && code->is_number()) {
            r.muted("終了コード " + std::to_string(static_cast<int>(code->n)));
        }
    }
    {
        ui::ViewBuilder r = b.row(6);
        const bool can_build = t.find("can_build") && t.find("can_build")->as_bool();
        if (can_build && !busy) r.button("ビルド", [this] { action("build"); });
        if (!busy) r.button("実行", [this] { action("run"); }, true);
        if (busy) r.button("停止", [this] { action("stop"); });
        r.button(watching_ ? "映像を止める" : "映像を見る", [this] {
            watching_ = !watching_;
            touch();
        });
    }
    b.video(watching_ ? "映像はブラウザの Studio (/shell/) に出ます" : "「映像を見る」で遠隔プレイの映像をつなぎます",
            watching_ ? stream_key(target_) : std::string(), 640, 360);
    if (const JsonValue* buttons = t.find("buttons"); buttons && buttons->is_array() && !buttons->a->empty()) {
        ui::ViewBuilder r = b.row(6);
        for (const auto& k : *buttons->a) {
            // Pressed and released by the browser page (shell.js) while the picture is connected.
            r.name_next("key-" + text_of(k, "key"));
            r.button(text_of(k, "label"), {});
        }
    }
    b.separator();
    {
        ui::ViewBuilder r = b.row(8);
        r.label("ログ");
        r.button("ログを更新", [this] { refresh_logs(); });
    }
    if (!slot_state(b, "logs")) return;
    const JsonValue* lines = slot("logs").result.json.find("lines");
    if (!lines || !lines->is_array() || lines->a->empty()) {
        b.muted("ログはまだありません");
        return;
    }
    ui::ViewBuilder log = b.panel(1);
    const std::size_t first = lines->a->size() > kLogLines ? lines->a->size() - kLogLines : 0;
    for (std::size_t i = first; i < lines->a->size(); ++i) {
        const JsonValue& l = (*lines->a)[i];
        log.clipped_label(text_of(l, "text"), 1100, text_of(l, "stream") == "stderr" ? &b.theme().warn : &b.theme().muted);
    }
}

} // namespace ergo::studio::views
