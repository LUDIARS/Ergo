#include "ergo/studio/registry/fetch.h"

#include <lorica/checked_size.hpp>
#include <portus/url.hpp>

#include <chrono>

namespace ergo::studio::registry {
namespace {

constexpr char kBase[] = "https://github.com/LUDIARS/ErgoPlugin/releases/latest/download/";
using Clock = std::chrono::steady_clock;

bool allowed(const std::string& raw, const char* file) {
    portus::Url url;
    if (raw.size() > 8192 || raw.find_first_of("@\\#") != std::string::npos
        || !portus::parse_url(raw, url) || url.scheme != "https" || url.port != 443) return false;
    if (url.host == "objects.githubusercontent.com" || url.host == "release-assets.githubusercontent.com") return true;
    if (url.host != "github.com") return false;
    if (url.target == "/LUDIARS/ErgoPlugin/releases/latest/download/" + std::string(file)) return true;
    constexpr std::string_view release_path = "/LUDIARS/ErgoPlugin/releases/download/";
    if (url.target.compare(0, release_path.size(), release_path) != 0) return false;
    const auto slash = url.target.find('/', release_path.size());
    if (slash == std::string::npos || url.target.compare(slash + 1, std::string::npos, file) != 0) return false;
    const auto tag = url.target.substr(release_path.size(), slash - release_path.size());
    return !tag.empty() && tag != "." && tag != ".." && tag.size() <= 128
        && tag.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-") == std::string::npos;
}

std::optional<std::string> fetch_asset(IAssetTransport& transport, const char* file,
                                       Clock::time_point deadline, std::size_t limit) {
    std::string url = kBase + std::string(file);
    for (int redirects = 0; redirects <= 4 && allowed(url, file); ++redirects) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        if (remaining < 1) return std::nullopt;
        auto response = transport.get(url, static_cast<int>(remaining), limit);
        if (response.status == 200 && lorica::contains_range(limit, 0, response.body.size())) return std::move(response.body);
        switch (response.status) {
        case 301: case 302: case 303: case 307: case 308:
            if (response.location.size() > 1 && response.location[0] == '/' && response.location[1] != '/') {
                portus::Url origin;
                if (!portus::parse_url(url, origin)) return std::nullopt;
                url = origin.origin() + response.location;
            } else {
                url = std::move(response.location);
            }
            break;
        default:
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<Release> fetch_release(IAssetTransport& transport, int timeout_ms, bool require_trust_assets) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    auto index = fetch_asset(transport, "index.signed.json", deadline, kDocumentLimit);
    if (!index) return std::nullopt;
    if (!require_trust_assets) return Release{std::move(*index), {}, {}};
    // Keep the index displayable when the artifacts used to establish trust are temporarily missing.
    auto revoked = fetch_asset(transport, "revoked.signed.json", deadline, kDocumentLimit);
    auto signer = fetch_asset(transport, "signing.pem", deadline, kCertificateLimit);
    return Release{std::move(*index), revoked.value_or(""), signer.value_or("")};
}

} // namespace ergo::studio::registry
