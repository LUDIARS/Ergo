/// ergo_log dummy plug — no-ops the entire API so hosts can link the
/// symbol surface without incurring any runtime cost.

#include "ergo/log/log.h"

namespace ergo::log {

namespace {
class NullSink final : public Sink {
public:
    void on_record(const Record&) override {}
};
} // namespace

Sink::~Sink() = default;

void set_level(Level)                       {}
Level level()                               { return Level::Info; }
void set_frame_provider(FrameProvider)      {}
void set_time_provider(TimeProvider)        {}
void vlog(Level, const char*, std::va_list) {}
void log (Level, const char*, ...)          {}
void add_sink(Sink*)                        {}
void remove_sink(Sink*)                     {}
Sink& console_sink()                        { static NullSink s; return s; }

} // namespace ergo::log
