#pragma once
/// Actio: task list / create / update (port of studio/clients/actio.ts).
/// Auth is Actio's local mode (loopback, no token) or a Cernere Bearer token.
/// Called from the Studio process, never with an Origin.
#include "endpoints.h"

namespace ergo::studio::plugins::studio {

class ActioClient {
public:
    ActioClient(EndpointResolver& ep, const ServiceCaller& caller, std::optional<std::string> token)
        : ep_(ep), caller_(caller), token_(std::move(token)) {}

    /// Tasks of a Cc project (`project` empty = all the caller may see):
    /// data = [task...]. Done tasks only with include_done.
    ServiceResult tasks(const std::string& project, bool include_done) const;
    ServiceResult task(const std::string& id) const;
    ServiceResult create(const JsonValue& input) const;
    ServiceResult update(const std::string& id, const JsonValue& input) const;
    /// Connectivity / auth check ({localMode: true} in local mode).
    ServiceResult me() const;

private:
    std::map<std::string, std::string> headers() const;

    EndpointResolver&          ep_;
    const ServiceCaller&       caller_;
    std::optional<std::string> token_;
};

} // namespace ergo::studio::plugins::studio
