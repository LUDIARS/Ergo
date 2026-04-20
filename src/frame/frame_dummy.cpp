/// ergo_frame dummy plug — provides no-op implementations for hosts
/// that want to link against the symbol surface without running the
/// real counter (editor tooling, codegen, etc.).

#include "ergo/frame/counter.h"
#include "ergo/frame/frame.h"

namespace ergo::frame {

Counter& Counter::instance() { static Counter c; return c; }

void Counter::tick()                              {}
void Counter::reset()                             {}
float Counter::fps() const                        { return 0.0f; }
void Counter::set_window_size(std::size_t /*n*/)  {}

std::string format_hud() { return {}; }

} // namespace ergo::frame
