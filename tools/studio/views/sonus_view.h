#pragma once
/// The サウンド tab of the sonus plugin: connection, and four pages —
/// ミキサー (fader / mute / solo per bus, undo / redo), プロファイラ (meters,
/// voices, timings, memory, log), 上書き (list, revert, revert all) and
/// 録画 (start / stop / save). Reads SonusPlugin::shown() and calls its
/// in-process API; never talks to Sonus itself.
///
/// The Studio UI has no slider or text field yet, so faders move by buttons
/// (-6 / -1 / +1 / +6 dB, 0 dB, off) and meters are drawn as bars.
#include "plugins/sonus/sonus_plugin.h"

#include "ergo/studio/ui/plugin_view.h"

namespace ergo::studio::views {

class SonusView final : public host::IPluginView {
public:
    enum class Page : uint8_t { Mixer = 0, Profiler = 1, Overrides = 2, Recording = 3 };

    /// `plugin` owns this view (SonusPlugin::attach_view) and outlives it.
    explicit SonusView(plugins::sonus::SonusPlugin& plugin) : plugin_(plugin) {}

    uint64_t view_revision() const override { return plugin_.revision() + local_revision_; }
    void     build(ui::ViewBuilder& b) override;

    Page page() const { return page_; }

private:
    void show(Page p);
    void build_mixer(ui::ViewBuilder& b, const plugins::sonus::SonusShown& s);
    void build_profiler(ui::ViewBuilder& b, const plugins::sonus::SonusShown& s);
    void build_overrides(ui::ViewBuilder& b, const plugins::sonus::SonusShown& s);
    void build_recording(ui::ViewBuilder& b, const plugins::sonus::SonusShown& s);

    plugins::sonus::SonusPlugin& plugin_;
    Page                         page_ = Page::Mixer;   ///< UI thread only
    std::string                  notice_;               ///< the last action's result, UI thread only
    uint64_t                     local_revision_ = 0;
};

} // namespace ergo::studio::views
