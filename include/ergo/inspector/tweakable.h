#pragma once

/// Tweakable — type-erased descriptor for a single inspectable variable.
///
/// Stores name, kind, metadata, and getter/setter callbacks. The Inspector
/// owns these and exposes them to clients. The setter is invoked on the
/// host's main thread (via `apply_pending_writes`), not on the network thread.

#include "ergo/inspector/types.h"

#include <functional>
#include <string>

namespace ergo::inspector {

struct Tweakable {
    std::string                       name;
    VarKind                           kind = VarKind::Bool;
    VarMeta                           meta;
    std::function<Value()>            getter;
    std::function<void(const Value&)> setter;
};

/// Clamp a numeric Value into its meta's [min, max] when both are non-zero.
/// Returns the (possibly modified) value. Strings/bools/colors/vec3 pass through.
Value clamp_to_meta(const Value& v, const VarMeta& meta);

} // namespace ergo::inspector
