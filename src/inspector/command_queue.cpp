#include "ergo/inspector/command_queue.h"

#include <utility>

namespace ergo::inspector {

void CommandQueue::push(WriteCommand cmd) {
    std::lock_guard<std::mutex> lk(mtx_);
    buf_.push_back(std::move(cmd));
}

std::vector<WriteCommand> CommandQueue::drain() {
    std::vector<WriteCommand> out;
    std::lock_guard<std::mutex> lk(mtx_);
    out.swap(buf_);
    return out;
}

std::size_t CommandQueue::size_approx() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return buf_.size();
}

void CommandQueue::clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    buf_.clear();
}

} // namespace ergo::inspector
