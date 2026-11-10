#pragma once
/// Endpoint resolution (port of studio/net/endpoints.ts, ST-INV-01). No port
/// lives in code:
///   1. Excubitor: EXCUBITOR_URL, else the `code: excubitor` port in
///      <LUDIARS_ROOT>/Excubitor/excubitor.catalog.yaml
///   2. other services: <CODE>_URL from Excubitor's GET /api/v1/topology
///   3. Curare: ERGO_CURARE_URL wins (a remote Cu Server)
/// Unresolved = "not connected"; no other port or host is tried.
#include "service_call.h"
#include "studio_config.h"

#include <chrono>
#include <map>
#include <mutex>

namespace ergo::studio::plugins::studio {

enum class Service { Concordia, Actio, Breviarium, Curare, Revisor, Excubitor, Cernere };
const char* service_key(Service s);        ///< "concordia", ...
const char* topology_env_name(Service s);  ///< "CONCORDIA_URL", ...
const std::vector<Service>& all_services();

/// Port of the `code: excubitor` entry (only its own 4-space `port:`, not
/// nested `ports:` entries); nullopt when absent.
std::optional<int> excubitor_port_from_catalog(const std::string& yaml);

/// EXCUBITOR_URL, else http://127.0.0.1:<catalog port>; nullopt when neither.
std::optional<std::string> resolve_excubitor_url(const StudioConfig& cfg);

struct EndpointSnapshot {
    std::string                                        resolved_at;
    std::string                                        topology = "not_configured";   ///< ok | unavailable | not_configured
    std::map<Service, std::optional<std::string>>      urls;
    std::map<std::string, std::string>                 all;   ///< every <CODE>_URL in the topology (CODE -> url)
};

/// Pick the Studio's services from a topology env (pure).
EndpointSnapshot pick_endpoints(const std::map<std::string, std::string>& env, const StudioConfig& cfg,
                                const std::optional<std::string>& excubitor);

/// Remembers a snapshot for 30 s. A failed refresh does not keep the last
/// good answer (no writes to a stale endpoint). Thread-safe.
class EndpointResolver {
public:
    EndpointResolver(const StudioConfig& cfg, const ServiceCaller& caller,
                     std::chrono::milliseconds ttl = std::chrono::seconds(30));

    EndpointSnapshot           snapshot(bool force = false);
    std::optional<std::string> url(Service s);

private:
    const StudioConfig&                   cfg_;
    const ServiceCaller&                  caller_;
    std::chrono::milliseconds             ttl_;
    std::mutex                            mtx_;
    std::optional<std::chrono::steady_clock::time_point> at_;
    EndpointSnapshot                      cache_;
};

std::string now_iso_utc();

} // namespace ergo::studio::plugins::studio
