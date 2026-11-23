#include "ergo/studio/ui/studio_app.h"


#include <algorithm>
#include <cstdio>

namespace ergo::studio::ui {

using ui_kit::LayoutMode;
using ui_kit::Node;
using ui_kit::Sizing;

namespace {

constexpr float kSidebarWidth = 230;
constexpr float kScrollStep   = 48;
constexpr float kBandHeight   = 48;   // the open project's band over the tabs

} // namespace

StudioApp::StudioApp(host::PluginHost& host, const ITextMeasure& measure, Theme theme, ViewLookup views,
                     project::Launcher* launcher)
    : host_(host), measure_(measure), theme_(std::move(theme)), views_(std::move(views)), launcher_(launcher) {}

bool StudioApp::showing_launcher() const {
    return launcher_ != nullptr && !launcher_->project().current();
}

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
    bind_.focused.clear();
    dirty_  = true;
}

uint64_t StudioApp::content_revision() const {
    if (active_ == 0) return 0;
    host::IPlugin* p = host_.plugins()[active_ - 1];
    host::IPluginView* v = view_of(*p, views_);
    return v ? v->view_revision() : 0;
}

bool StudioApp::edit_focused(const InputState& in) {
    if (bind_.focused.empty() || (in.text.empty() && in.keys.empty())) return false;
    const auto it = std::find_if(bind_.fields.begin(), bind_.fields.end(),
                                 [&](const FieldBinding& f) { return f.path == bind_.focused; });
    if (it == bind_.fields.end() || !it->value) {
        bind_.focused.clear();
        return true;
    }
    std::string& v = *it->value;
    for (char c : in.text) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 && !(it->multiline && c == '\n')) continue;   // control characters (pasted tabs, CR)
        v.push_back(c);
    }
    ClickFn submit;
    for (Key k : in.keys) {
        switch (k) {
        case Key::Backspace:
            if (!v.empty()) {
                std::size_t n = v.size() - 1;
                while (n > 0 && (static_cast<unsigned char>(v[n]) & 0xC0) == 0x80) --n;   // whole code point
                v.erase(n);
            }
            break;
        case Key::Enter:
            if (it->multiline) v.push_back('\n');
            else submit = it->on_submit;
            break;
        case Key::Escape: bind_.focused.clear(); break;
        case Key::Delete:
        case Key::Tab: break;   // no caret movement: the caret is always at the end
        }
    }
    if (submit) submit();   // may rebuild the view's data; the field pointers are refreshed by rebuild()
    return true;
}

void StudioApp::frame(const InputState& in, float dt) {
    if (edit_focused(in)) dirty_ = true;
    // A press anywhere drops the focus; a field's own click (on release) takes it again.
    if (in.down && !was_down_ && !bind_.focused.empty()) {
        bind_.focused.clear();
        dirty_ = true;
    }
    was_down_ = in.down;
    if (in.scroll != 0) {
        scroll_ = std::max(0.0f, scroll_ - in.scroll * kScrollStep);
        dirty_  = true;
    }
    since_health_ += dt;
    const bool health_due = since_health_ * 1000 >= static_cast<float>(health_interval_.count());
    // Health changes without a revision: the overview and generic tabs re-read it periodically.
    const bool shows_health = active_ == 0 || !view_of(*host_.plugins()[active_ - 1], views_);
    // Periodic refreshes wait until the button is up (the layout must not move under a press).
    const bool periodic = health_due && shows_health && !in.down;
    if (periodic) since_health_ = 0;
    const bool launcher_moved = launcher_ != nullptr && launcher_->revision() != built_launcher_revision_;
    if (dirty_ || launcher_moved || content_revision() != built_revision_ || periodic) rebuild();
    const std::string focus_before = bind_.focused;
    ctx_.feed_input({in.x, in.y, in.down});
    ctx_.update(dt);
    if (bind_.focused != focus_before) dirty_ = true;   // a field took the focus: redraw it with the caret
}

void StudioApp::rebuild() {
    dirty_          = false;
    built_revision_ = content_revision();
    built_launcher_revision_ = launcher_ ? launcher_->revision() : 0;
    bind_.clear();

    ui_kit::Document doc;
    Node& root      = doc.root;
    root.name       = "Root";
    root.size       = {static_cast<float>(vw_), static_cast<float>(vh_)};
    root.style.fill = theme_.background;
    if (showing_launcher()) rebuild_launcher(root);
    else rebuild_tabs(root, launcher_ ? kBandHeight : 0);
    bind_handlers(std::move(doc));
}

namespace {

/// A clipped viewport with a column moved up by the scroll offset.
Node scroll_view(const char* name, float x, float y, float w, float h, float scroll) {
    Node view;
    view.name     = name;
    view.position = {x, y};
    view.size     = {w, h};
    view.clip     = true;
    Node content;
    content.name            = "Content";
    content.position        = {0, -scroll};
    content.size            = {w, 0};
    content.layout.mode     = LayoutMode::Vertical;
    content.layout.gap      = 8;
    content.layout.pad_l = content.layout.pad_r = content.layout.pad_t = content.layout.pad_b = 16;
    content.layout.h_sizing = Sizing::Hug;
    view.children.push_back(std::move(content));
    return view;
}

} // namespace

void StudioApp::rebuild_launcher(Node& root) {
    // The launcher alone: no tab list until a project is open.
    root.children.push_back(scroll_view("Launcher", 0, 0, static_cast<float>(vw_), static_cast<float>(vh_), scroll_));
    ViewNode body;
    ViewBuilder b(body, measure_, theme_);
    build_launcher(b, *launcher_, launcher_input_, true, [this] { dirty_ = true; });
    emit_ui_kit(body, root.children[0].children[0], "Root/Launcher/Content", measure_, theme_, bind_);
}

void StudioApp::rebuild_tabs(Node& root, float top) {
    // Left: one line per plugin (16+ plugins do not fit a horizontal bar). Right: the tab.
    const float side_w = std::min(kSidebarWidth, static_cast<float>(vw_) * 0.4f);
    const float h      = std::max(1.0f, static_cast<float>(vh_) - top);
    Node bar;
    bar.name             = "Tabs";
    bar.position         = {0, top};
    bar.size             = {side_w, h};
    bar.style.fill       = theme_.panel;
    bar.layout.mode      = LayoutMode::Vertical;
    bar.layout.gap       = 2;
    bar.layout.pad_l = bar.layout.pad_r = 6;
    bar.layout.pad_t     = 8;
    bar.clip             = true;
    root.children.push_back(std::move(bar));
    root.children.push_back(scroll_view("View", side_w, top, static_cast<float>(vw_) - side_w, h, scroll_));

    {
        ViewNode tabs;
        ViewBuilder tb(tabs, measure_, theme_);
        build_tab_list(tb, host_, active_, [this](std::size_t i) { select_tab(i); });
        emit_ui_kit(tabs, root.children[0], "Root/Tabs", measure_, theme_, bind_);
    }
    {
        ViewNode body;
        ViewBuilder bb(body, measure_, theme_);
        if (active_ == 0) {
            build_overview(bb, host_);
        } else {
            host::IPlugin* p = host_.plugins()[active_ - 1];
            if (host::IPluginView* v = view_of(*p, views_)) v->build(bb);
            else build_generic(bb, *p);
        }
        emit_ui_kit(body, root.children[1].children[0], "Root/View/Content", measure_, theme_, bind_);
    }
    const auto project = launcher_ ? launcher_->project().current() : std::nullopt;
    if (!project) return;
    Node band;
    band.name          = "Project";
    band.size          = {static_cast<float>(vw_), top};
    band.style.fill    = theme_.panel_alt;
    band.layout.mode   = LayoutMode::Vertical;
    band.layout.pad_l  = band.layout.pad_r = 12;
    band.layout.pad_t  = 8;
    band.clip          = true;
    root.children.push_back(std::move(band));
    ViewNode body;
    ViewBuilder bb(body, measure_, theme_);
    build_project_bar(bb, *project, [this] {
        launcher_->close();
        dirty_ = true;
    });
    emit_ui_kit(body, root.children.back(), "Root/Project", measure_, theme_, bind_);
}

void StudioApp::bind_handlers(ui_kit::Document doc) {
    // Keep the context (its hover / pressed state) across rebuilds: replacing it
    // would lose a press that started before the rebuild, so the click never fires.
    ctx_.clear_handlers();
    ctx_.set_viewport(vw_, vh_);
    ctx_.set_document(std::move(doc));
    for (auto& c : bind_.clicks) {
        ClickFn fn = c.fn;
        ctx_.on(c.path, ui_kit::EventKind::Click, [fn](ui_kit::Event& e) {
            e.propagate = false;
            fn();
        });
    }
    for (auto& sl : bind_.sliders) {
        SetFractionFn fn = sl.fn;
        const std::string path = sl.path;
        ctx_.on(path, ui_kit::EventKind::Click, [this, fn, path](ui_kit::Event& e) {
            e.propagate = false;
            const ui_kit::Node* track = ctx_.find(path);
            if (track == nullptr || track->resolved_rect.w <= 0) return;
            const float f = (e.x - track->resolved_rect.x) / track->resolved_rect.w;
            fn(f < 0 ? 0.0f : (f > 1 ? 1.0f : f));
        });
    }
}

} // namespace ergo::studio::ui
