#include "log_view.h"

#include "json_render.h"

#include <chrono>

namespace ergo::studio::views {

namespace {
const ui_kit::Color* level_color(const ui::Theme& t, const std::string& lvl) {
    if (lvl == "ERR" || lvl == "FTL") return &t.error;
    if (lvl == "WRN") return &t.warn;
    if (lvl == "DBG" || lvl == "TRC") return &t.muted;
    return nullptr;
}
} // namespace

LogView::LogView(host::IPlugin& plugin) : ExplorerView(plugin) {
    poller_ = std::thread([this] {
        while (!stop_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (!follow_.load() || stop_.load()) continue;
            std::string q;
            {
                std::lock_guard<std::mutex> lk(query_mtx_);
                q = query_;
            }
            if (!q.empty()) get("lines", q);
        }
    });
}

LogView::~LogView() {
    stop_.store(true);
    if (poller_.joinable()) poller_.join();
}

void LogView::on_first_show() { get("apps", "/api/apps"); }

std::string LogView::lines_query() const {
    std::string q = "/api/lines?app=" + url_encode(app_) + "&limit=" + url_encode(limit_.empty() ? "300" : limit_);
    if (!levels_.empty()) q += "&level=" + url_encode(levels_);
    if (!text_.empty()) q += "&q=" + url_encode(text_);
    if (!frame_min_.empty()) q += "&frame_min=" + url_encode(frame_min_);
    if (!frame_max_.empty()) q += "&frame_max=" + url_encode(frame_max_);
    return q;
}

void LogView::reload_lines() {
    if (app_.empty()) return;
    const std::string q = lines_query();
    {
        std::lock_guard<std::mutex> lk(query_mtx_);
        query_ = q;
    }
    get("lines", q);
}

void LogView::render(ui::ViewBuilder& b) {
    b.title(plugin_.info().title);
    b.muted(plugin_.info().description);
    {
        ui::ViewBuilder r = b.row(6);
        r.button("\xE3\x82\xA2\xE3\x83\x97\xE3\x83\xAA\xE3\x82\x92\xE6\x9B\xB4\xE6\x96\xB0", [this] { get("apps", "/api/apps"); });   // アプリを更新
        r.button(follow_.load() ? "\xE8\xBF\xBD\xE5\xBE\x93\xE4\xB8\xAD" : "\xE8\xBF\xBD\xE5\xBE\x93\xE3\x81\x99\xE3\x82\x8B", [this] {   // 追従中 / 追従する
            follow_.store(!follow_.load());
            touch();
        }, follow_.load());
    }
    ui::ViewBuilder apps = b.panel(3);
    if (slot_state(apps, "apps")) {
        const JsonValue* list = slot("apps").result.json.find("apps");
        if (!list || !list->is_array() || list->a->empty()) {
            apps.muted("\xE6\x8E\xA5\xE7\xB6\x9A\xE3\x81\x97\xE3\x81\x9F\xE3\x82\xA8\xE3\x83\xB3\xE3\x82\xB8\xE3\x83\xB3\xE3\x81\xAF\xE3\x81\x82\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93 (ergo_log_live \xE3\x81\x8C /log/ws \xE3\x81\xB8\xE3\x81\xA4\xE3\x81\xAA\xE3\x81\x90)");   // 接続したエンジンはありません (ergo_log_live が /log/ws へつなぐ)
        } else {
            for (const auto& a : *list->a) {
                const std::string name = text_of(a, "app");
                const bool connected = a.find("connected") && a.find("connected")->as_bool();
                apps.list_item(name, name == app_, [this, name] {
                    app_ = name;
                    reload_lines();
                }, (connected ? "\xE6\x8E\xA5\xE7\xB6\x9A\xE4\xB8\xAD  " : "\xE5\x88\x87\xE6\x96\xAD  ") + text_of(a, "lines") + " lines");   // 接続中 / 切断
            }
        }
    }
    if (app_.empty()) return;
    {
        ui::ViewBuilder f = b.row(6);
        auto submit = [this] { reload_lines(); };
        f.text_field(levels_, "level (ERR,WRN)", 160, submit);
        f.text_field(text_, "\xE6\x96\x87\xE5\xAD\x97\xE5\x88\x97", 260, submit);   // 文字列
        f.text_field(frame_min_, "frame \xE2\x89\xA5", 100, submit);
        f.text_field(frame_max_, "frame \xE2\x89\xA4", 100, submit);
        f.text_field(limit_, "limit", 70, submit);
        f.button("\xE7\xB5\x9E\xE3\x82\x8A\xE8\xBE\xBC\xE3\x82\x80", submit, true);   // 絞り込む
    }
    ui::ViewBuilder lines = b.panel(1);
    if (!slot_state(lines, "lines")) return;
    const JsonValue& res = slot("lines").result.json;
    const JsonValue* arr = res.find("lines");
    if (const JsonValue* d = res.find("dropped"); d && d->as_number() > 0) {
        lines.label("dropped: " + scalar_text(*d), &b.theme().warn);
    }
    if (!arr || !arr->is_array() || arr->a->empty()) {
        lines.muted("\xE8\xA1\x8C\xE3\x81\x8C\xE3\x81\x82\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93");   // 行がありません
        return;
    }
    for (const auto& l : *arr->a) {
        const std::string lvl = text_of(l, "lvl");
        ui::ViewBuilder r = lines.row(8);
        r.label(lvl, level_color(b.theme(), lvl), b.theme().small_size);
        r.label(text_of(l, "frame"), &b.theme().muted, b.theme().small_size);
        r.clipped_label(text_of(l, "msg"), 1000, level_color(b.theme(), lvl));
    }
}

} // namespace ergo::studio::views
