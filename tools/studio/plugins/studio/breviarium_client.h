#pragma once
/// Breviarium (Br): project status summaries (startup / sprint / PDCA /
/// class review). Br answers from its cache; only refresh goes to the
/// sources. Port of studio/clients/breviarium.ts.
#include "endpoints.h"

namespace ergo::studio::plugins::studio {

class BreviariumClient {
public:
    BreviariumClient(EndpointResolver& ep, const ServiceCaller& caller) : ep_(ep), caller_(caller) {}

    ServiceResult projects() const;   ///< data = [project...]
    ServiceResult overview(const std::string& code) const;
    ServiceResult refresh(const std::string& code) const;   ///< 4x timeout
    ServiceResult health() const;

private:
    EndpointResolver&    ep_;
    const ServiceCaller& caller_;
};

} // namespace ergo::studio::plugins::studio
