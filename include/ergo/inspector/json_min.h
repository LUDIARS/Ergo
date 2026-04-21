#pragma once

/// Re-export of the common JSON codec under the inspector namespace.
///
/// The implementation lives at `ergo/common/json_min.{h,cpp}` and is
/// shared with ergo_bind (and any future WS-protocol module). The
/// namespace alias below keeps existing `ergo::inspector::jsonm::...`
/// call sites compiling unchanged.

#include "ergo/common/json_min.h"

namespace ergo::inspector {
namespace jsonm = ::ergo::common::jsonm;
} // namespace ergo::inspector
