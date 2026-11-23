#pragma once
/// Native tab of "play" (spec/feature/tool/play.md): choose a target, build /
/// run / stop it, its log, and the remote-play picture. The picture is a
/// video element with stream key "play:<target>" while watching: the browser
/// shell (/shell/, shell.js) opens WebRTC through /play/ws for that key and
/// sends pointer / keys and the operation buttons (named "key-<name>") to the
/// game. The window has no WebRTC and shows the frame with a note.
#include "api_view.h"

#include <string>

namespace ergo::studio::views {

class PlayView final : public ApiView {
public:
    explicit PlayView(host::IPlugin& plugin) : ApiView(plugin) {}

    /// Stream key of the picture for `target` (what shell.js looks for).
    static std::string stream_key(const std::string& target) { return "play:" + target; }

protected:
    void render(ui::ViewBuilder& b) override;
    void on_first_show() override;

private:
    void action(const std::string& name);
    void refresh_logs();
    void render_target(ui::ViewBuilder& b, const JsonValue& t);

    std::string target_;     ///< selected target
    bool        watching_ = false;
};

/// "停止中" / "ビルド中" / "実行中" / "失敗" for the plugin's state names.
std::string play_state_text(const std::string& state);

} // namespace ergo::studio::views
