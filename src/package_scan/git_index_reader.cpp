#include "ergo/package_scan/git_index_reader.h"

#include <cstddef>
#include <cstdint>

namespace ergo::package_scan {

namespace {

constexpr std::uint32_t kGitlinkMode = 0160000;
constexpr std::size_t   kHeaderSize  = 12;
constexpr std::size_t   kStatSize    = 40;   // ctime..size の 10 語
constexpr std::size_t   kShaSize     = 20;
constexpr std::uint16_t kExtendedFlag = 0x4000;
constexpr std::uint16_t kNameMask     = 0x0fff;

std::uint32_t be32(const std::string& b, std::size_t at) {
    return (std::uint32_t(std::uint8_t(b[at])) << 24) | (std::uint32_t(std::uint8_t(b[at + 1])) << 16) |
           (std::uint32_t(std::uint8_t(b[at + 2])) << 8) | std::uint32_t(std::uint8_t(b[at + 3]));
}

std::uint16_t be16(const std::string& b, std::size_t at) {
    return std::uint16_t((std::uint8_t(b[at]) << 8) | std::uint8_t(b[at + 1]));
}

std::string hex(const std::string& b, std::size_t at, std::size_t n) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < n; ++i) {
        const auto v = std::uint8_t(b[at + i]);
        out += digits[v >> 4];
        out += digits[v & 0x0f];
    }
    return out;
}

bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

} // namespace

bool parse_index_gitlinks(const std::string& b, std::map<std::string, std::string>& gitlinks,
                          std::string* error) {
    gitlinks.clear();
    if (b.size() < kHeaderSize || b.compare(0, 4, "DIRC") != 0) {
        return fail(error, "not a git index");
    }
    const std::uint32_t version = be32(b, 4);
    if (version != 2 && version != 3) return fail(error, "unsupported git index version");
    const std::uint32_t count = be32(b, 8);

    std::size_t at = kHeaderSize;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t start = at;
        if (at + kStatSize + kShaSize + 2 > b.size()) return fail(error, "truncated git index");
        const std::uint32_t mode = be32(b, at + 24);
        const std::string   sha  = hex(b, at + kStatSize, kShaSize);
        const std::uint16_t flags = be16(b, at + kStatSize + kShaSize);
        at += kStatSize + kShaSize + 2;
        if (flags & kExtendedFlag) {
            if (version < 3) return fail(error, "extended flag in a version 2 index");
            at += 2;
        }
        std::size_t name_len = flags & kNameMask;
        if (name_len == kNameMask) {   // 長い名前は NUL まで
            const auto nul = b.find('\0', at);
            if (nul == std::string::npos) return fail(error, "truncated git index");
            name_len = nul - at;
        }
        if (at + name_len > b.size()) return fail(error, "truncated git index");
        const std::string name = b.substr(at, name_len);
        at += name_len;
        // 版 2 / 3 のエントリは NUL 1〜8 個で 8 バイト境界に揃う。
        const std::size_t used = at - start;
        at = start + ((used + 8) / 8) * 8;
        if (at > b.size()) return fail(error, "truncated git index");
        if (mode == kGitlinkMode) gitlinks[name] = sha;
    }
    return true;
}

} // namespace ergo::package_scan
