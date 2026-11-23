#pragma once
/// Generic drawing of JSON answers: scalars as key / value lines, objects as
/// nested panels, arrays as a capped list. Used by the endpoint explorer and
/// as the fallback inside the dedicated views.
#include "ergo/common/json_min.h"
#include "ergo/studio/ui/view_builder.h"

#include <string>

namespace ergo::studio::views {

using ergo::common::jsonm::JsonValue;

/// "12", "true", "text", or the compact JSON of a container.
std::string scalar_text(const JsonValue& v);
/// Field of an object as text ("" when missing).
std::string text_of(const JsonValue& v, const char* key);

struct JsonRenderLimits {
    int         max_depth = 4;
    std::size_t max_items = 50;    ///< per array
    float       max_width = 900;   ///< long scalars are cut
};

void render_json(ui::ViewBuilder& b, const JsonValue& v, const JsonRenderLimits& lim = {});

} // namespace ergo::studio::views
