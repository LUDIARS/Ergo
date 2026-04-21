#pragma once

/// Re-export of the common JSON codec under the bind namespace.
///
/// The implementation lives at `ergo/common/json_min.{h,cpp}` and is
/// shared with ergo_inspector. The namespace alias below keeps existing
/// `ergo::bind::jsonm::...` call sites compiling unchanged.

#include "ergo/common/json_min.h"

namespace ergo::bind {
namespace jsonm = ::ergo::common::jsonm;
} // namespace ergo::bind
