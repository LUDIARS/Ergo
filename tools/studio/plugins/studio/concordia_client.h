#pragma once
/// Concordia (Cc): registered projects and AI implementation requests
/// (delegation). Port of studio/clients/concordia.ts. Cc's HTTP API is a
/// loopback-only admin API. Records stay JSON (they go to the editor as is).
#include "endpoints.h"

namespace ergo::studio::plugins::studio {

class ConcordiaClient {
public:
    ConcordiaClient(EndpointResolver& ep, const ServiceCaller& caller) : ep_(ep), caller_(caller) {}

    /// Admin list (with repo origin and Revisor workflow): data = [project...].
    ServiceResult projects() const;
    ServiceResult register_project(const JsonValue& input) const;
    /// Active templates only: data = [template...].
    ServiceResult templates() const;
    ServiceResult invoke(const JsonValue& req) const;   ///< 4x timeout
    ServiceResult runs(int limit = 100) const;          ///< data = [run...]
    ServiceResult run(const std::string& id) const;     ///< data = run

private:
    EndpointResolver&    ep_;
    const ServiceCaller& caller_;
};

/// `data[key]` when it is an array, `data` itself when it is one, else [].
JsonValue list_of(const JsonValue& data, const char* key);
/// `data[key]` when it is an object, else `data`.
JsonValue unwrap(const JsonValue& data, const char* key);

} // namespace ergo::studio::plugins::studio
