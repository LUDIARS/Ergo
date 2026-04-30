#include "ergo/health/health.h"

// Header-only behavior; this TU exists so we ship a real .a/.lib with
// `add_library(ergo_health STATIC ...)`.

namespace ergo::health {

// Nothing else: all logic lives inline in the header.

}  // namespace ergo::health
