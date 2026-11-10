#pragma once
/// Query parameters of GET /log/api/lines -> filter + limit.
///   level=ERR,WRN  since=<seq>  frame_min / frame_max  q=<text>  limit=<n>
#include "line_ring.h"

#include <map>
#include <string>
#include <variant>

namespace ergo::studio::plugins::log {

constexpr std::size_t kDefaultLinesLimit = 1000;

struct LinesQuery {
    LineFilter  filter;
    std::size_t limit = kDefaultLinesLimit;   ///< capped at kRingCapacity
};

/// The query, or the error message for HTTP 400.
std::variant<LinesQuery, std::string> parse_lines_query(const std::map<std::string, std::string>& q);

} // namespace ergo::studio::plugins::log
