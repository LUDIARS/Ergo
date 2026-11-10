#pragma once
/// Curare (Cu): asset versions and the acquisition plan of chosen versions
/// (port of studio/clients/curare.ts). Bearer token from ERGO_CURARE_TOKEN;
/// never stored or shown (ST-INV-09).
#include "endpoints.h"

namespace ergo::studio::plugins::studio {

class CurareClient {
public:
    CurareClient(EndpointResolver& ep, const ServiceCaller& caller, std::optional<std::string> token)
        : ep_(ep), caller_(caller), token_(std::move(token)) {}

    bool has_token() const { return token_.has_value(); }

    ServiceResult me() const;
    ServiceResult projects() const;   ///< data = [...]
    ServiceResult folders(const std::string& project_id) const;
    ServiceResult assets(const std::string& project_id, const std::string& folder_id) const;
    ServiceResult versions(const std::string& asset_id) const;
    /// Plan for the versions (dependency closure included); known hashes skip downloads.
    ServiceResult plan(const std::vector<std::string>& version_ids, const std::vector<std::string>& known_hashes) const;

private:
    ServiceResult get(const std::string& path) const;
    std::map<std::string, std::string> headers() const;

    EndpointResolver&          ep_;
    const ServiceCaller&       caller_;
    std::optional<std::string> token_;
};

} // namespace ergo::studio::plugins::studio
