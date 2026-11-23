#include "ergo/studio/tela/studio_declaration.h"

#include <algorithm>
#include <utility>

namespace ergo::studio::tela_shell {

namespace {

constexpr float kSidebarWidth = 230;

/// 公開名からはこのタブを操作できません (ローカルの画面から操作してください)
const char* const kLockedNotice =
    "\xE5\x85\xAC\xE9\x96\x8B\xE5\x90\x8D\xE3\x81\x8B\xE3\x82\x89\xE3\x81\xAF\xE3\x81\x93\xE3\x81\xAE\xE3\x82\xBF\xE3\x83"
    "\x96\xE3\x82\x92\xE6\x93\x8D\xE4\xBD\x9C\xE3\x81\xA7\xE3\x81\x8D\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93 (\xE3\x83\xAD"
    "\xE3\x83\xBC\xE3\x82\xAB\xE3\x83\xAB\xE3\x81\xAE\xE7\x94\xBB\xE9\x9D\xA2\xE3\x81\x8B\xE3\x82\x89\xE6\x93\x8D\xE4"
    "\xBD\x9C\xE3\x81\x97\xE3\x81\xA6\xE3\x81\x8F\xE3\x81\xA0\xE3\x81\x95\xE3\x81\x84)";

} // namespace

StudioDeclaration::StudioDeclaration(host::PluginHost& host, ui::ViewLookup views, ui::Theme theme)
    : host_(host), views_(std::move(views)), theme_(std::move(theme)), tela_theme_(tela_theme(theme_)) {}

bool StudioDeclaration::loopback_only(const std::string& plugin_id) {
    return plugin_id == "studio" || plugin_id == "lesson";
}

std::string StudioDeclaration::active_plugin() const {
    return active_ == 0 ? std::string() : host_.plugins()[active_ - 1]->info().id;
}

std::string StudioDeclaration::content_id() const {
    return "content/" + (active_ == 0 ? std::string("overview") : active_plugin());
}

void StudioDeclaration::select_tab(std::size_t i) {
    if (i >= tab_count() || i == active_) return;
    active_ = i;
    dirty_  = true;
}

uint64_t StudioDeclaration::content_revision() const {
    if (active_ == 0) return 0;
    host::IPluginView* v = ui::view_of(*host_.plugins()[active_ - 1], views_);
    return v ? v->view_revision() : 0;
}

const ::tela::Document& StudioDeclaration::document(ScreenAccess access) const {
    return access == ScreenAccess::local ? full_ : published_;
}

bool StudioDeclaration::refresh(float dt) {
    since_health_ += dt;
    const bool health_due = since_health_ * 1000 >= static_cast<float>(health_interval_.count());
    // Health changes without a revision: the overview and generic tabs re-read it periodically.
    const bool shows_health = active_ == 0 || !ui::view_of(*host_.plugins()[active_ - 1], views_);
    const bool periodic     = health_due && shows_health;
    if (health_due) since_health_ = 0;
    if (!dirty_ && content_revision() == built_revision_ && !periodic) return false;
    rebuild();
    return true;
}

void StudioDeclaration::rebuild() {
    dirty_          = false;
    built_revision_ = content_revision();

    ui::ViewNode tabs;
    {
        ui::ViewBuilder b(tabs, measure_, theme_);
        ui::build_tab_list(b, host_, active_, [this](std::size_t i) { select_tab(i); });
    }
    ui::ViewNode body;
    {
        ui::ViewBuilder b(body, measure_, theme_);
        if (active_ == 0) {
            ui::build_overview(b, host_);
        } else {
            host::IPlugin* p = host_.plugins()[active_ - 1];
            if (host::IPluginView* v = ui::view_of(*p, views_)) v->build(b);
            else ui::build_generic(b, *p);
        }
    }
    full_      = {};
    published_ = {};
    declare(full_, tabs, body, false);
    declare(published_, tabs, body, loopback_only(active_plugin()));
    ++revision_;
}

void StudioDeclaration::declare(::tela::Document& doc, const ui::ViewNode& tabs, const ui::ViewNode& body,
                                bool locked) {
    const auto touched = [this] { touch(); };
    const EmitOptions nav{measure_, theme_, true, touched};
    const EmitOptions content{measure_, theme_, !locked, touched};

    ::tela::Layout side;
    side.positioned = true;
    side.width      = kSidebarWidth;
    side.padding    = 6;
    side.gap        = 2;
    doc.scroll(tabs_id(), [&] { emit_children(tabs, doc, tabs_id(), nav); }, side);

    ::tela::Layout main;
    main.positioned = true;
    main.x          = kSidebarWidth;
    main.padding    = 16;
    main.gap        = 8;
    const std::string id = content_id();
    doc.scroll(id, [&] {
        if (locked) {
            ::tela::Layout l;
            l.padding = 2;
            doc.status(id + "/locked", kLockedNotice, ::tela::Tone::warning, l);
        }
        emit_children(body, doc, id, content);
    }, main);
}

} // namespace ergo::studio::tela_shell
