#include "line_ring.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ergo::studio::plugins::log {
namespace {

std::string ascii_lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

} // namespace

bool matches_filter(const StoredLine& line, const LineFilter& f, const std::string& needle) {
    if (f.levels && f.levels->count(line.lvl) == 0) return false;
    if (f.since && static_cast<double>(line.seq) <= *f.since) return false;
    if (f.frame_min && line.frame < *f.frame_min) return false;
    if (f.frame_max && line.frame > *f.frame_max) return false;
    if (!needle.empty() && ascii_lower(line.msg).find(needle) == std::string::npos) return false;
    return true;
}

LineRing::LineRing(std::size_t capacity) : buf_(std::max<std::size_t>(capacity, 1)) {}

const StoredLine& LineRing::push(const LogLine& line) {
    StoredLine stored;
    static_cast<LogLine&>(stored) = line;
    stored.seq = next_seq_++;
    std::size_t slot;
    if (count_ < buf_.size()) {
        slot = (head_ + count_) % buf_.size();
        ++count_;
    } else {
        slot  = head_;
        head_ = (head_ + 1) % buf_.size();
    }
    buf_[slot] = std::move(stored);
    return buf_[slot];
}

std::vector<StoredLine> LineRing::query(const LineFilter& f, std::size_t limit) const {
    // seq is contiguous inside the ring, so `since` maps to an index.
    std::size_t start = 0;
    if (f.since) {
        const double idx = std::floor(*f.since) - static_cast<double>(first_seq()) + 1;
        start = idx <= 0 ? 0 : static_cast<std::size_t>(std::min(idx, static_cast<double>(count_)));
    }
    const std::string needle = ascii_lower(f.q);
    std::vector<StoredLine> out;
    for (std::size_t i = count_; i > start && out.size() < limit; --i) {
        const StoredLine& line = buf_[(head_ + i - 1) % buf_.size()];
        if (matches_filter(line, f, needle)) out.push_back(line);
    }
    std::reverse(out.begin(), out.end());
    return out;
}

} // namespace ergo::studio::plugins::log
