#pragma once
/// The Studio window's UI, GPU independent: a tab per plugin (plus an
/// overview of every plugin's health), a scrollable content area, and the
/// ui_kit context that turns it into a draw list. The shell feeds input and
/// draws draw_list(); tests drive it directly.
#include "ergo/studio/host/plugin_host.h"
#include "ergo/studio/ui/plugin_view.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ergo::studio::ui {

enum class Key : uint8_t { Backspace, Delete, Enter, Escape, Tab };

struct InputState {
    float x = 0, y = 0;
    bool  down   = false;
    float scroll = 0;          ///< wheel steps since the last frame (+ = up)
    std::string      text;     ///< UTF-8 typed (and pasted) since the last frame
    std::vector<Key> keys;     ///< editing keys pressed since the last frame, in order
};

/// Views for plugins that do not draw their own (IPlugin::view() == nullptr):
/// the exe attaches native tabs to built-in and external plugins by id.
using ViewLookup = std::function<host::IPluginView*(host::IPlugin&)>;

class StudioApp {
public:
    /// `host` owns the plugins; it outlives the app.
    StudioApp(host::PluginHost& host, const ITextMeasure& measure, Theme theme = {}, ViewLookup views = {});

    void set_viewport(int w, int h);
    /// Rebuild when needed, then layout + input + draw list.
    void frame(const InputState& in, float dt);

    const std::vector<ui_kit::DrawCmd>& draw_list() const { return ctx_.draw_list(); }
    const Theme&                        theme() const { return theme_; }

    /// Tabs: 0 = overview, then one per plugin in load order.
    std::size_t tab_count() const { return 1 + host_.plugins().size(); }
    std::size_t active_tab() const { return active_; }
    void        select_tab(std::size_t i);
    /// How often the overview re-reads health (it is cheap, but not free).
    void        set_health_interval(std::chrono::milliseconds ms) { health_interval_ = ms; }
    /// Path of the focused text field ("" = none).
    const std::string& focused_field() const { return bind_.focused; }

private:
    host::IPluginView* view_of(host::IPlugin& p) const;
    /// Apply typed text / keys to the focused field; true when something changed.
    bool edit_focused(const InputState& in);
    void rebuild();
    void build_tabs(ViewBuilder& bar);
    void build_overview(ViewBuilder& b);
    void build_generic(ViewBuilder& b, host::IPlugin& p);
    uint64_t content_revision() const;

    host::PluginHost&         host_;
    const ITextMeasure&       measure_;
    Theme                     theme_;
    ViewLookup                views_;
    ui_kit::UIContext         ctx_;
    Bindings                  bind_;
    bool                      was_down_ = false;
    int                       vw_ = 1280, vh_ = 800;
    std::size_t               active_ = 0;
    float                     scroll_ = 0;
    bool                      dirty_  = true;
    uint64_t                  built_revision_ = ~0ull;
    std::chrono::milliseconds health_interval_{1000};
    float                     since_health_ = 0;
};

} // namespace ergo::studio::ui
