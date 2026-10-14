#include "ergo/package_watch/ref_listing.h"

#include "ergo/package_manager/declaration_check.h"

#include <cstdint>
#include <cstdio>
#include <sstream>

namespace ergo::package_watch {

namespace {

const std::string kHeads   = "refs/heads/";
const std::string kOrigin  = "refs/remotes/origin/";
const std::string kTags    = "refs/tags/";
const std::string kPeeled  = "^{}";

bool starts_with(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

std::string trim(const std::string& s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

void add_line(const std::string& sha, const std::string& ref, RefLayout layout, RefListing& out,
              std::map<std::string, std::string>& peeled) {
    const std::string& branch_prefix = layout == RefLayout::Remote ? kHeads : kOrigin;
    if (starts_with(ref, branch_prefix)) {
        const std::string name = ref.substr(branch_prefix.size());
        if (!name.empty() && name != "HEAD") out.heads[name] = sha;
        return;
    }
    if (!starts_with(ref, kTags)) return;
    std::string name = ref.substr(kTags.size());
    if (name.size() > kPeeled.size() &&
        name.compare(name.size() - kPeeled.size(), kPeeled.size(), kPeeled) == 0) {
        name.erase(name.size() - kPeeled.size());
        peeled[name] = sha;
        return;
    }
    if (!name.empty()) out.tags[name] = sha;
}

void fnv(std::uint64_t& h, const std::string& s) {
    for (const unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
}

} // namespace

void parse_ref_lines(const std::string& text, RefLayout layout, RefListing& out) {
    std::map<std::string, std::string> peeled;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.size() < 42) continue;
        const std::string sha = line.substr(0, 40);
        if (!ergo::package_manager::is_full_sha(sha) || (line[40] != '\t' && line[40] != ' ')) continue;
        add_line(sha, trim(line.substr(41)), layout, out, peeled);
    }
    // 注釈つきタグはタグ object ではなく commit を見る。
    for (const auto& [name, sha] : peeled) {
        if (out.tags.count(name)) out.tags[name] = sha;
    }
}

std::string listing_fingerprint(const RefListing& listing) {
    std::uint64_t h = 1469598103934665603ULL;
    for (const auto& [name, sha] : listing.heads) fnv(h, "h " + name + " " + sha + "\n");
    for (const auto& [name, sha] : listing.tags) fnv(h, "t " + name + " " + sha + "\n");
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
    return buf;
}

} // namespace ergo::package_watch
