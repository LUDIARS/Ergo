#include "ergo/studio/ui/studio_app.h"

#include "ergo/common/json_min.h"
#include "ergo/studio/host/health_report.h"

#include <algorithm>
#include <cstdio>

namespace ergo::studio::ui {

using ui_kit::LayoutMode;
using ui_kit::Node;
using ui_kit::Sizing;
namespace jm = ergo::common::jsonm;

namespace {

constexpr float kTabBarHeight = 44;
constexpr float kScrollStep   = 48;

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

StudioApp::StudioApp(host::PluginHost& host, const ITextMeasure& measure, Theme theme)
    : host_(host), measure_(measure), theme_(std::move(theme)) {}

void StudioApp::set_viewport(int w, int h) {
    if (w == vw_ && h == vh_) return;
    vw_ = std::max(1, w);
    vh_ = std::max(1, h);
    ctx_.set_viewport(vw_, vh_);
    dirty_ = true;
}

void StudioApp::select_tab(std::size_t i) {
    if (i >= tab_count() || i == active_) return;
    active_ = i;
    scroll_ = 0;
    dirty_  = true;
}

uint64_t StudioApp::content_revision() const {
    if (active_ == 0) return 0;
    host::IPlugin* p = host_.plugins()[active_ - 1];
    host::IPluginView* v = p->view();
    return v ? v->view_revision() : 0;
}

void StudioApp::frame(const InputState& in, float dt) {
    if (in.scroll != 0) {
        scroll_ = std::max(0.0f, scroll_ - in.scroll * kScrollStep);
        dirty_  = true;
    }
    since_health_ += dt;
    const bool health_due = since_health_ * 1000 >= static_cast<float>(health_interval_.count());
    // Health changes without a revision: the overview and generic tabs re-read it periodically.
    const bool shows_health = active_ == 0 || !host_.plugins()[active_ - 1]->view();
    // Periodic refreshes wait until the button is up (the layout must not move under a press).
    const bool periodic = health_due && shows_health && !in.down;
    if (periodic) since_health_ = 0;
    if (dirty_ || content_revision() != built_revision_ || periodic) rebuild();
    ctx_.feed_input({in.x, in.y, in.down});
    ctx_.update(dt);
}

void StudioApp::rebuild() {
    dirty_          = false;
    built_revision_ = content_revision();
    clicks_.clear();

    ui_kit::Document doc;
    Node& root      = doc.root;
    root.name       = "Root";
    root.size       = {static_cast<float>(vw_), static_cast<float>(vh_)};
    root.style.fill = theme_.background;

    Node bar;
    bar.name              = "Tabs";
    bar.size              = {static_cast<float>(vw_), kTabBarHeight};
    bar.style.fill        = theme_.panel;
    bar.layout.mode       = LayoutMode::Horizontal;
    bar.layout.gap        = 4;
    bar.layout.pad_l      = 8;
    bar.layout.pad_t      = 6;
    bar.layout.align_cross = ui_kit::AlignCross::Center;
    bar.clip              = true;
    root.children.push_back(std::move(bar));

    // Content: clipped viewport + a column moved up by the scroll offset.
    Node view;
    view.name     = "View";
    view.position = {0, kTabBarHeight};
    view.size     = {static_cast<float>(vw_), static_cast<float>(vh_) - kTabBarHeight};
    view.clip     = true;
    Node content;
    content.name            = "Content";
    content.position        = {0, -scroll_};
    content.size            = {static_cast<float>(vw_), 0};
    content.layout.mode     = LayoutMode::Vertical;
    content.layout.gap      = 8;
    content.layout.pad_l = content.layout.pad_r = content.layout.pad_t = content.layout.pad_b = 16;
    content.layout.h_sizing = Sizing::Hug;
    view.children.push_back(std::move(content));
    root.children.push_back(std::move(view));

    {
        ViewBuilder tabs(root.children[0], "Root/Tabs", measure_, theme_, clicks_);
        build_tabs(tabs);
    }
    ViewBuilder body(root.children[1].children[0], "Root/View/Content", measure_, theme_, clicks_);
    if (active_ == 0) {
        build_overview(body);
    } else {
        host::IPlugin* p = host_.plugins()[active_ - 1];
        if (host::IPluginView* v = p->view()) v->build(body);
        else build_generic(body, *p);
    }

    // Keep the context (its hover / pressed state) across rebuilds: replacing it
    // would lose a press that started before the rebuild, so the click never fires.
    ctx_.clear_handlers();
    ctx_.set_viewport(vw_, vh_);
    ctx_.set_document(std::move(doc));
    for (auto& c : clicks_) {
        ClickFn fn = c.fn;
        ctx_.on(c.path, ui_kit::EventKind::Click, [fn](ui_kit::Event& e) {
            e.propagate = false;
            fn();
        });
    }
}

void StudioApp::build_tabs(ViewBuilder& bar) {
    auto tab = [&](std::size_t i, const std::string& title) {
        bar.button(title, [this, i] { select_tab(i); }, i == active_);
    };
    tab(0, "\xE6\xA6\x82\xE8\xA6\x81");   // 概要
    for (std::size_t i = 0; i < host_.plugins().size(); ++i) {
        const auto& info = host_.plugins()[i]->info();
        tab(i + 1, info.title);
    }
}

void StudioApp::build_overview(ViewBuilder& b) {
    b.title("Ergo Studio");
    b.muted("\xE8\xAA\xAD\xE3\x81\xBF\xE8\xBE\xBC\xE3\x82\x93\xE3\x81\xA0\xE3\x83\x97\xE3\x83\xA9\xE3\x82\xB0\xE3\x82\xA4\xE3\x83\xB3\xE3\x81\xA8\xE7\x8A\xB6\xE6\x85\x8B");   // 読み込んだプラグインと状態
    for (host::IPlugin* p : host_.plugins()) {
        const auto h = host::plugin_health(*p);
        ViewBuilder panel = b.panel(4);
        panel.status(health_ok(h), p->info().title + "  (" + p->info().id + ")");
        if (const jm::JsonValue* err = h.find("err")) panel.muted(scalar_text(*err));
        else if (const jm::JsonValue* w = h.find("warning")) panel.muted(scalar_text(*w));
    }
    for (const auto& r : host_.rejected()) {
        ViewBuilder panel = b.panel(4);
        panel.status(false, "\xE8\xAA\xAD\xE3\x81\xBF\xE8\xBE\xBC\xE3\x82\x81\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xA3\xE3\x81\x9F\xE5\xA4\x96\xE9\x83\xA8\xE3\x83\x91\xE3\x83\x83\xE3\x82\xAF: " + r.source);   // 読み込めなかった外部パック
        panel.muted(r.reason);
    }
}

void StudioApp::build_generic(ViewBuilder& b, host::IPlugin& p) {
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
