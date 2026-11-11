#include "services_view.h"

#include "json_render.h"

namespace ergo::studio::views {
namespace jm = ergo::common::jsonm;

namespace {

std::string state_word(const std::string& state, bool running) {
    if (running) return "稼働中";
    if (state == "stopped" || state.empty()) return "停止";
    return state.c_str();
}

} // namespace

void ServicesView::on_first_show() {
    get("health", "/api/health");
    get("list", "/api/services");
}

void ServicesView::control(const std::string& code, const std::string& action, bool force) {
    jm::JsonValue body = jm::JsonValue::make_object();
    body.set("action", jm::JsonValue::make_string(action));
    if (force) body.set("force", jm::JsonValue::make_bool(true));
    fetch("control", "POST", "/api/services/" + code + "/control", jm::serialize(body), [this, code, action](const ApiResult& r) {
        if (r.status == 409 && action != "stop") {
            contested_ = code;
            set_notice(r.error_text(), false);
            return;
        }
        contested_.clear();
        set_notice(r.ok() ? code + ": " + action + " を受け付けました" : r.error_text(), r.ok());
        get("list", "/api/services");
    });
}

void ServicesView::show_logs(const std::string& code) {
    selected_ = code;
    get("logs", "/api/services/" + code + "/logs");
}

void ServicesView::render(ui::ViewBuilder& b) {
    b.title(plugin_.info().title);
    b.muted(plugin_.info().description);
    {
        const Slot& h = slot("health");
        if (h.loaded && h.result.json.find("ok") && !h.result.json.find("ok")->as_bool()) {
            b.label(text_of(h.result.json, "err"), &b.theme().warn);
        }
    }
    render_notice(b);
    {
        ui::ViewBuilder r = b.row(6);
        r.button("更新", [this] { get("list", "/api/services"); });
    }
    if (!contested_.empty()) {
        ui::ViewBuilder r = b.row(6);
        r.label(contested_ + " は別の作業がテスト中です。", &b.theme().warn);
        const std::string code = contested_;
        r.button("それでも起動", [this, code] { control(code, "start", true); });
        r.button("やめる", [this] {
            contested_.clear();
            touch();
        });
    }
    if (!slot_state(b, "list")) return;
    const auto& list = slot("list").result.json;
    if (!list.is_array() || list.a->empty()) {
        b.paragraph("表示するサービスがありません。ワークスペースのプロフィール (services) を確認してください。");
        return;
    }
    ui::ViewBuilder panel = b.panel(4);
    for (const auto& s : *list.a) {
        const std::string code = text_of(s, "code");
        const bool running = s.find("running") && s.find("running")->as_bool();
        ui::ViewBuilder r = panel.row(8);
        r.clipped_label(text_of(s, "name").empty() ? code : text_of(s, "name") + " (" + code + ")", 300);
        r.label(state_word(text_of(s, "state"), running), running ? &b.theme().ok : nullptr);
        if (running) {
            r.button("停止", [this, code] { control(code, "stop", false); });
            r.button("再起動", [this, code] { control(code, "restart", false); });
        } else {
            r.button("起動", [this, code] { control(code, "start", false); }, true);
        }
        r.button("ログ", [this, code] { show_logs(code); }, code == selected_);
    }
    if (selected_.empty()) return;
    b.separator();
    b.label(selected_ + " のログ");
    if (!slot_state(b, "logs")) return;
    render_json(b, slot("logs").result.json);
}

} // namespace ergo::studio::views
