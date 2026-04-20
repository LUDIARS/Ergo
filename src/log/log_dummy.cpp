/// ergo_log dummy plug — no-ops the entire API so hosts can link the
/// symbol surface without incurring any runtime cost.

#include "ergo/log/log.h"

namespace ergo::log {

void set_level(Level)                       {}
Level level()                               { return Level::Info; }
void set_frame_provider(FrameProvider)      {}
void vlog(Level, const char*, std::va_list) {}
void log (Level, const char*, ...)          {}

} // namespace ergo::log
