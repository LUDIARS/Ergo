#pragma once
/// The connection map (port of studio/domain/connection-map.ts): nodes for
/// the services the Studio talks to and edges for the known links between
/// them (Cc <-> Actio project / task sync, Br -> Actio sprint totals,
/// Cu -> Cr authorisation, ...). Pure.
#include "endpoints.h"

namespace ergo::studio::plugins::studio {

struct ProbeResult {
    Service                    key = Service::Concordia;
    std::optional<std::string> url;
    bool                       ok = false;
    std::optional<FailureKind> failure;
    std::string                detail;
    std::string                checked_at;
    std::optional<std::string> last_ok_at;
    std::string                auth;   ///< how it authenticates (never a secret)
};

/// ok | degraded | down | not_configured | unauthorized
const char* node_state(const ProbeResult* p);

/// {nodes:[{id,label,role,state,url,detail,auth,lastOkAt}], edges:[{from,to,label,active}]}
JsonValue build_connection_map(const std::vector<ProbeResult>& probes);

JsonValue to_json_value(const ProbeResult& p);

} // namespace ergo::studio::plugins::studio
