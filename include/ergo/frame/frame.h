#pragma once

/// Free-function shortcuts around the single Counter singleton, plus a
/// one-line HUD formatter for debug overlays.

#include <cstdint>
#include <string>

#include "ergo/frame/counter.h"

namespace ergo::frame {

inline void     tick()        { Counter::instance().tick(); }
inline void     reset()       { Counter::instance().reset(); }
inline uint64_t count()       { return Counter::instance().count(); }
inline float    fps()         { return Counter::instance().fps(); }
inline float    dt_seconds()  { return Counter::instance().dt_seconds(); }

/// One-line debug string: `"F<count> FPS=<fps> dt=<ms>ms"`. Safe to
/// call from any frame (shows `FPS=0.0` until the window fills).
std::string format_hud();

} // namespace ergo::frame
