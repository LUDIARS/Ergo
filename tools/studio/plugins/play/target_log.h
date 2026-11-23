#pragma once
/// The output lines of one target's build / run: the last 2000, numbered so a
/// client asks for what came after the line it last saw (PLAY-INV-04).
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace ergo::studio::plugins::play {

struct LogLine {
    uint64_t    n = 0;
    bool        is_stderr = false;
    std::string text;
};

class TargetLog {
public:
    static constexpr std::size_t kMaxLines = 2000;
    static constexpr std::size_t kMaxLine  = 4096;

    void push(bool is_stderr, std::string_view text);
    /// Lines numbered >= since (the oldest kept when since is older), and the next number.
    std::vector<LogLine> since(uint64_t since, uint64_t& next) const;

private:
    std::deque<LogLine> lines_;
    uint64_t            next_ = 0;
};

} // namespace ergo::studio::plugins::play
