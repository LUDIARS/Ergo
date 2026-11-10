#pragma once
/// JSON bodies of /api/health and /api/plugins.
#include "ergo/studio/host/plugin_loader.h"

#include <string>
#include <vector>

namespace ergo::studio::host {

/// One plugin's health with "id" and "title" added. A throw or a result
/// without a boolean "ok" becomes {ok:false, err:...}; it never fails the
/// whole report. A plugin without IPluginHealth is {ok:true}.
JsonValue plugin_health(IPlugin& p);

/// {ok, plugins:[...], rejected:[{id, source, ok:false, err}]}. "ok" is false
/// when any plugin is unhealthy or any external entry was rejected.
std::string health_report(const std::vector<IPlugin*>& plugins, const std::vector<RejectedPlugin>& rejected);

/// {plugins:[{id, title, description, icon, ws}]}. "ws" only for IPluginSocket.
std::string plugin_list(const std::vector<IPlugin*>& plugins);

} // namespace ergo::studio::host
