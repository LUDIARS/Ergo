#pragma once
/// HTTP routes of the "sonus" plugin on the Studio's own (loopback) server, for
/// setups without the window and for scripts (spec/feature/tool/sonus.md):
///   GET  /sonus/api/state                 what the tab shows (sonus_shown_json)
///   POST /sonus/api/connect               {}  open the live link to the configured game
///   POST /sonus/api/disconnect            {}
///   POST /sonus/api/mixer/bus             {"bus":"<key>", "fader_db":<n> | "mute":<bool> | "solo":<bool>}
///   POST /sonus/api/mixer/undo            {}
///   POST /sonus/api/mixer/redo            {}
///   POST /sonus/api/overrides/revert      {"key":"<override key>"}
///   POST /sonus/api/overrides/revert-all  {}
///   POST /sonus/api/recording/start       {}
///   POST /sonus/api/recording/stop        {}
///   POST /sonus/api/recording/save        {}  -> {"path": ...} under ERGO_SONUS_RECORDING_DIR
/// Mutating calls pass the Studio's mutation guard (ST-INV-03). The target of the
/// link and the recording directory come from the settings only, never a request.
#include "sonus_plugin.h"

#include "ergo/studio/net/http_message.h"

namespace ergo::studio::plugins::sonus {

net::HttpResponse handle_sonus_route(SonusPlugin& plugin, const net::HttpRequest& req, const std::string& sub,
                                     const host::LogFn& log);

} // namespace ergo::studio::plugins::sonus
