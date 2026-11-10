#include "resource_sync.h"

#include "../common/safe_path.h"

#include <cctype>
#include <regex>

namespace ergo::studio::plugins::studio {

const char* sync_action_name(SyncAction a) {
    switch (a) {
        case SyncAction::SkipSame: return "skip_same";
        case SyncAction::Download: return "download";
        case SyncAction::Conflict: return "conflict";
        case SyncAction::Unavailable: return "unavailable";
    }
    return "";
}

SyncAction decide_sync_action(const std::string& content_hash, bool needs_download, bool has_download_url,
                              const std::optional<std::string>& local_hash) {
    if (local_hash && *local_hash == content_hash) return SyncAction::SkipSame;
    if (local_hash) return SyncAction::Conflict;
    if (!has_download_url) return needs_download ? SyncAction::Unavailable : SyncAction::SkipSame;
    return SyncAction::Download;
}

std::optional<std::string> safe_file_name(const std::string& name) {
    std::string base;
    for (char c : name) {
        const auto u = static_cast<unsigned char>(c);
        const bool bad = c == '\\' || c == '/' || u < 0x20 || c == '<' || c == '>' || c == ':' || c == '"' || c == '|' ||
                         c == '?' || c == '*';
        base.push_back(bad ? '_' : c);
    }
    const auto b = base.find_first_not_of(" \t\r\n");
    base = b == std::string::npos ? std::string() : base.substr(b, base.find_last_not_of(" \t\r\n") - b + 1);
    static const std::regex reserved(R"(^(con|prn|aux|nul|com\d|lpt\d)(\..*)?$)", std::regex::icase);
    if (base.empty() || base == "." || base == ".." || std::regex_match(base, reserved)) return std::nullopt;
    if (base.size() > 200) {
        // Cut on a UTF-8 boundary.
        std::size_t cut = 200;
        while (cut > 0 && (static_cast<unsigned char>(base[cut]) & 0xC0) == 0x80) --cut;
        base = base.substr(0, cut);
    }
    return base;
}

std::optional<std::filesystem::path> resolve_inside(const std::filesystem::path& repo, const std::string& subdir) {
    const std::filesystem::path p = std::filesystem::u8path(subdir);
    if (p.is_absolute() || p.has_root_name() || p.has_root_directory()) return std::nullopt;
    return resolve_within(repo, subdir.empty() ? std::string(".") : subdir);
}

std::string to_content_hash(const std::string& hex) {
    std::string h = hex;
    for (char& c : h) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return "sha256:" + h;
}

} // namespace ergo::studio::plugins::studio
