#pragma once

#include <cstdint>
#include <ctime>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ergo::studio::registry {

inline constexpr std::size_t kDocumentLimit = 1024 * 1024;
inline constexpr std::size_t kCertificateLimit = 64 * 1024;

struct Release {
    std::string index;
    std::string revoked;
    std::string signing_pem;
};

struct Provenance {
    std::string id, publisher, fingerprint, checked_at;
    std::uint64_t serial = 0;
};

struct Entry {
    std::string id, title, publisher, kind, source, rev, reason;
    std::vector<std::string> domains, styles;
};

struct Trust {
    std::string reason;
    std::string fingerprint;
    std::uint64_t serial = 0;
    std::vector<Entry> entries;
    std::set<std::string> revoked_signers;
    std::set<std::pair<std::string, std::string>> revoked_versions;

    bool verified() const { return reason.empty() && serial != 0; }
    bool permits(const Provenance& provenance, const std::string& rev) const;
};

Trust verify_release(const Release& release, const std::string& root_pem,
                     std::uint64_t minimum_serial, std::time_t now);

} // namespace ergo::studio::registry
