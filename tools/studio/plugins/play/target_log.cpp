#include "target_log.h"

namespace ergo::studio::plugins::play {

void TargetLog::push(bool is_stderr, std::string_view text) {
    lines_.push_back({next_++, is_stderr, std::string(text.substr(0, kMaxLine))});
    while (lines_.size() > kMaxLines) lines_.pop_front();
}

std::vector<LogLine> TargetLog::since(uint64_t since, uint64_t& next) const {
    next = next_;
    std::vector<LogLine> out;
    for (const auto& line : lines_) {
        if (line.n >= since) out.push_back(line);
    }
    return out;
}

} // namespace ergo::studio::plugins::play
