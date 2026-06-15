#include "ergo/math/math.h"

// Header-only behavior: most ergo_math logic lives inline in the headers.
// This TU exists so we ship a real .a/.lib with add_library(ergo_math STATIC ...).

namespace ergo::math {

/// Returns the library version string.
const char* version() noexcept {
    return "ergo_math 0.1.0";
}

}  // namespace ergo::math
