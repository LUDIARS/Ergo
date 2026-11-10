#include "plugin_source.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace ergo::studio::plugins::studio {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

} // namespace

std::optional<std::string> normalize_subpath(const std::string& raw) {
    std::string p = trim(raw);
    while (p.rfind("./", 0) == 0) {
        p.erase(0, 2);
        while (!p.empty() && p[0] == '/') p.erase(0, 1);
    }
    while (!p.empty() && p.back() == '/') p.pop_back();
    if (p.empty() || p.find('\\') != std::string::npos || p[0] == '/') return std::nullopt;
    if (p.size() >= 2 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':') return std::nullopt;
    static const std::regex part(R"(^[A-Za-z0-9._-]+$)");
    std::size_t start = 0;
    while (start <= p.size()) {
        const auto end = p.find('/', start);
        const std::string s = p.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (s.empty() || s == "." || s == ".." || !std::regex_match(s, part)) return std::nullopt;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return p;
}

PluginSourceParse parse_plugin_source(const std::string& spec, const std::vector<std::string>& allowed) {
    PluginSourceParse out;
    static const std::regex shape(R"(^([^/\s:@]+)/([^/\s:@]+):([^@\s]+)(?:@(\S+))?$)");
    static const std::regex name(R"(^[A-Za-z0-9](?:[A-Za-z0-9._-]{0,98}[A-Za-z0-9_])?$)");
    static const std::regex rev(R"(^[A-Za-z0-9._/-]{1,200}$)");
    std::smatch m;
    const std::string s = trim(spec);
    if (!std::regex_match(s, m, shape)) {
        out.reason = "`owner/repo:subpath[@rev]` の形で指定してください";
        return out;
    }
    const std::string owner = m[1], repo = m[2];
    if (!std::regex_match(owner, name) || !std::regex_match(repo, name)) {
        out.reason = "owner / repo 名が不正です";
        return out;
    }
    const bool ok_owner = std::any_of(allowed.begin(), allowed.end(), [&](const std::string& o) { return lower(o) == lower(owner); });
    if (!ok_owner) {
        out.reason = "取得元 " + owner + " は許可リストにありません (ERGO_PLUGIN_OWNERS)";
        return out;
    }
    const auto sub = normalize_subpath(m[3]);
    if (!sub) {
        out.reason = "subpath が不正です (リポ内の相対パス)";
        return out;
    }
    const std::string r = m[4].matched ? m[4].str() : std::string("HEAD");
    if (!std::regex_match(r, rev) || r.find("..") != std::string::npos) {
        out.reason = "rev が不正です";
        return out;
    }
    out.source = PluginSource{owner, repo, *sub, r};
    return out;
}

bool is_full_sha(const std::string& s) {
    static const std::regex sha(R"(^[0-9a-f]{40}$)");
    return std::regex_match(s, sha);
}

std::string pack_id(const PluginSource& s) { return s.owner + "/" + s.repo + ":" + s.subpath; }

} // namespace ergo::studio::plugins::studio
