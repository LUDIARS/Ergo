#pragma once

#include "../common/profile_source.h"
#include "ergo/studio/host/plugin.h"

namespace ergo::studio::plugins::asset_import {

struct CaptureRequest;
using RequestPreview = std::function<bool(CaptureRequest)>;

std::unique_ptr<host::IPlugin> make_asset_import_plugin(std::shared_ptr<const IProfileSource> profiles,
                                                       RequestPreview preview, const host::LogFn& log);

} // namespace ergo::studio::plugins::asset_import
