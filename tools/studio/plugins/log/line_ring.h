#pragma once
/// Bounded line store of one app (port of log/ring.ts): the newest
/// `capacity` lines, each with the next seq (1, 2, 3, ...).
#include "log_protocol.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ergo::studio::plugins::log {

constexpr std::size_t kRingCapacity = 10000;

struct LineFilter {
    std::optional<std::set<std::string>> levels;   ///< every level when unset
    std::optional<double>                since;    ///< seq > since
    std::string                          q;        ///< case-insensitive (ASCII) substring of msg
    std::optional<double>                frame_min;   ///< inclusive
    std::optional<double>                frame_max;
};

bool matches_filter(const StoredLine& line, const LineFilter& f, const std::string& lowered_needle);

class LineRing {
public:
    /// `capacity` >= 1 (0 is treated as 1).
    explicit LineRing(std::size_t capacity = kRingCapacity);

    std::size_t capacity() const { return buf_.size(); }
    std::size_t size() const { return count_; }
    /// seq of the newest line; 0 before the first push.
    uint64_t    last_seq() const { return next_seq_ - 1; }
    /// seq of the oldest line held (last_seq + 1 when empty).
    uint64_t    first_seq() const { return next_seq_ - count_; }

    /// Append, evicting the oldest when full. Returns the stored line.
    const StoredLine& push(const LogLine& line);

    /// Matching lines, oldest first; only the newest `limit` when more match.
    std::vector<StoredLine> query(const LineFilter& f = {}, std::size_t limit = SIZE_MAX) const;

private:
    std::vector<StoredLine> buf_;
    std::size_t             head_     = 0;
    std::size_t             count_    = 0;
    uint64_t                next_seq_ = 1;
};

} // namespace ergo::studio::plugins::log
