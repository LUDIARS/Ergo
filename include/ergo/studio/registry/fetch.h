#pragma once

#include "ergo/studio/registry/trust.h"

#include <memory>
#include <optional>

namespace ergo::studio::registry {

struct AssetResponse {
    int status = 0;
    std::string location, body;
};

class IAssetTransport {
public:
    virtual ~IAssetTransport() = default;
    virtual AssetResponse get(const std::string& url, int timeout_ms, std::size_t limit) = 0;
};

std::optional<Release> fetch_release(IAssetTransport& transport, int timeout_ms, bool require_trust_assets = true);
std::unique_ptr<IAssetTransport> make_portus_transport();

} // namespace ergo::studio::registry
