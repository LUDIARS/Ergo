// LineSplitter: the pure part of live_process.h (shared by every backend).
#include "ergo/studio/platform/live_process.h"

namespace ergo::studio::platform {

void LineSplitter::push(std::string_view chunk, const std::function<void(std::string_view)>& emit) {
    for (const char c : chunk) {
        if (c == '\n') {
            if (!cutting_) {
                if (!pending_.empty() && pending_.back() == '\r') pending_.pop_back();
                emit(pending_);
            }
            pending_.clear();
            cutting_ = false;
            continue;
        }
        if (cutting_) continue;
        pending_.push_back(c);
        if (pending_.size() >= max_line_) {
            emit(pending_);
            pending_.clear();
            cutting_ = true;
        }
    }
}

void LineSplitter::finish(const std::function<void(std::string_view)>& emit) {
    if (!cutting_ && !pending_.empty()) {
        if (pending_.back() == '\r') pending_.pop_back();
        emit(pending_);
    }
    pending_.clear();
    cutting_ = false;
}

} // namespace ergo::studio::platform
