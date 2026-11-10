#include "lines_query.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <optional>

namespace ergo::studio::plugins::log {
namespace {

std::string trim_upper(const std::string& s) {
    const auto b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return {};
    std::string t = s.substr(b, s.find_last_not_of(" \t") - b + 1);
    for (char& c : t) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return t;
}

/// A finite number spanning the whole string, else nullopt.
std::optional<double> to_number(const std::string& s) {
    if (s.empty()) return std::nullopt;
    char*        end = nullptr;
    const double v   = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size() || !std::isfinite(v)) return std::nullopt;
    return v;
}

std::string get(const std::map<std::string, std::string>& q, const char* key) {
    const auto it = q.find(key);
    return it == q.end() ? std::string() : it->second;
}

} // namespace

std::variant<LinesQuery, std::string> parse_lines_query(const std::map<std::string, std::string>& q) {
    LinesQuery out;
    if (const std::string level = get(q, "level"); !level.empty()) {
        std::set<std::string> levels;
        std::size_t start = 0;
        while (true) {
            const auto        comma = level.find(',', start);
            const std::string part  = level.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            const std::string tag   = trim_upper(part);
            if (!is_level_tag(tag)) return "unknown level \"" + part + "\" (use ERR,WRN,INF,DBG)";
            levels.insert(tag);
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        out.filter.levels = std::move(levels);
    }
    const std::pair<const char*, std::optional<double> LineFilter::*> numbers[] = {
        {"since", &LineFilter::since}, {"frame_min", &LineFilter::frame_min}, {"frame_max", &LineFilter::frame_max}};
    for (const auto& [param, member] : numbers) {
        const std::string raw = get(q, param);
        if (raw.empty()) continue;
        const auto n = to_number(raw);
        if (!n) return std::string(param) + " must be a number";
        out.filter.*member = *n;
    }
    out.filter.q = get(q, "q");

    if (const std::string raw = get(q, "limit"); !raw.empty()) {
        const auto n = to_number(raw);
        if (!n || *n < 1 || std::floor(*n) != *n) return std::string("limit must be a positive integer");
        out.limit = *n > static_cast<double>(kRingCapacity) ? kRingCapacity : static_cast<std::size_t>(*n);
    }
    return out;
}

} // namespace ergo::studio::plugins::log
