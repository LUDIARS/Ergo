#include "ergo/package_watch/semver.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace ergo::package_watch {

namespace {

bool all_digits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(),
                                     [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string part;
    std::istringstream in(s);
    while (std::getline(in, part, sep)) out.push_back(part);
    if (!s.empty() && s.back() == sep) out.push_back({});
    return out;
}

int number_at(const Semver& v, std::size_t i) {
    return i < v.numbers.size() ? v.numbers[i] : 0;
}

/// pre-release の 1 段: 数字だけなら数で、 それ以外は文字列で比べる。 数字は文字列より前。
int compare_identifier(const std::string& a, const std::string& b) {
    const bool na = all_digits(a);
    const bool nb = all_digits(b);
    if (na && nb) {
        if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
        return a.compare(b) < 0 ? -1 : (a == b ? 0 : 1);
    }
    if (na != nb) return na ? -1 : 1;
    return a.compare(b) < 0 ? -1 : (a == b ? 0 : 1);
}

} // namespace

std::optional<Semver> parse_semver(const std::string& tag) {
    if (tag.size() < 2 || tag[0] != 'v') return std::nullopt;
    std::string body = tag.substr(1);
    Semver out;
    if (const auto dash = body.find('-'); dash != std::string::npos) {
        out.pre = body.substr(dash + 1);
        body.erase(dash);
        if (out.pre.empty()) return std::nullopt;
    }
    const auto parts = split(body, '.');
    if (parts.empty() || parts.size() > 3) return std::nullopt;
    for (const auto& p : parts) {
        if (!all_digits(p) || p.size() > 9) return std::nullopt;
        out.numbers.push_back(std::stoi(p));
    }
    return out;
}

int compare_semver(const Semver& a, const Semver& b) {
    for (std::size_t i = 0; i < 3; ++i) {
        const int x = number_at(a, i);
        const int y = number_at(b, i);
        if (x != y) return x < y ? -1 : 1;
    }
    if (a.pre == b.pre) return 0;
    if (a.pre.empty()) return 1;
    if (b.pre.empty()) return -1;
    const auto pa = split(a.pre, '.');
    const auto pb = split(b.pre, '.');
    for (std::size_t i = 0; i < pa.size() && i < pb.size(); ++i) {
        if (const int c = compare_identifier(pa[i], pb[i]); c != 0) return c;
    }
    if (pa.size() == pb.size()) return 0;
    return pa.size() < pb.size() ? -1 : 1;
}

} // namespace ergo::package_watch
