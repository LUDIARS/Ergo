#pragma once
/// SonusShown as JSON (GET /sonus/api/state): the same content as the tab.
#include "sonus_shown.h"

#include "../common/json_util.h"

namespace ergo::studio::plugins::sonus {

JsonValue sonus_shown_json(const SonusShown& s);

} // namespace ergo::studio::plugins::sonus
