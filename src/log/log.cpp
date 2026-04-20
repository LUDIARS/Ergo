#include "ergo/log/log.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace ergo::log {

namespace {

struct State {
    Level         threshold = Level::Info;
    FrameProvider provider  = nullptr;
    std::mutex    print_mtx;
};

State& state() {
    static State s;
    return s;
}

uint64_t current_frame() {
    auto& s = state();
    if (!s.provider) return 0;
    // Protect against exceptions / user-supplied callbacks.
    try {
        return s.provider();
    } catch (...) {
        return 0;
    }
}

std::FILE* stream_for(Level lvl) {
    return (lvl == Level::Error || lvl == Level::Warn) ? stderr : stdout;
}

} // namespace

void set_level(Level threshold) {
    state().threshold = threshold;
}

Level level() {
    return state().threshold;
}

void set_frame_provider(FrameProvider fn) {
    state().provider = std::move(fn);
}

void vlog(Level lvl, const char* fmt, std::va_list args) {
    auto& s = state();
    if (static_cast<int>(lvl) > static_cast<int>(s.threshold)) return;
    if (!fmt) return;

    // Render payload into a stack buffer, grow if truncated.
    char stack_buf[512];
    char* buf = stack_buf;
    std::string heap_buf;

    std::va_list copy;
    va_copy(copy, args);
    const int needed = std::vsnprintf(stack_buf, sizeof(stack_buf), fmt, copy);
    va_end(copy);

    if (needed > 0 && static_cast<size_t>(needed) >= sizeof(stack_buf)) {
        heap_buf.resize(static_cast<size_t>(needed) + 1);
        std::vsnprintf(heap_buf.data(), heap_buf.size(), fmt, args);
        buf = heap_buf.data();
    }

    const uint64_t f = current_frame();
    const auto tag   = level_tag(lvl);

    std::FILE* out = stream_for(lvl);
    std::lock_guard<std::mutex> g(s.print_mtx);
    std::fprintf(out, "[F%08llu][%.*s] %s\n",
                 static_cast<unsigned long long>(f),
                 static_cast<int>(tag.size()), tag.data(),
                 buf);
    std::fflush(out);
}

void log(Level lvl, const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    vlog(lvl, fmt, args);
    va_end(args);
}

} // namespace ergo::log
