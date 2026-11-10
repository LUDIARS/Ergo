#pragma once
/// The studio plugin's dependencies, assembled in one place (port of
/// studio/deps.ts): settings, endpoint resolution and every client.
#include "actio_client.h"
#include "breviarium_client.h"
#include "concordia_client.h"
#include "connection_map.h"
#include "curare_client.h"
#include "git_client.h"
#include "plugin_installer.h"
#include "revisor_client.h"

#include <map>
#include <memory>
#include <mutex>
#include <variant>

namespace ergo::studio::plugins::studio {

struct StudioDeps {
    StudioConfig                              cfg;
    std::shared_ptr<platform::IHttpClient>    http;
    std::unique_ptr<ServiceCaller>            caller;
    std::unique_ptr<EndpointResolver>         endpoints;
    std::unique_ptr<ConcordiaClient>          cc;
    std::unique_ptr<ActioClient>              actio;
    std::unique_ptr<BreviariumClient>         br;
    std::unique_ptr<CurareClient>             cu;
    std::unique_ptr<GitClient>                git;
    std::unique_ptr<RevisorClient>            revisor;
    std::unique_ptr<PluginInstaller>          installer;
    std::mutex                                last_ok_mtx;
    std::map<Service, std::string>            last_ok;   ///< for the connection map

    /// `sdk_dir` = <exe dir>/sdk (plugin_abi.h for building packs).
    static std::unique_ptr<StudioDeps> create(StudioConfig cfg, std::shared_ptr<platform::IHttpClient> http,
                                              std::shared_ptr<platform::IProcessRunner> runner,
                                              const std::filesystem::path& sdk_dir);
};

/// One request with its path split ("/api/tasks/7/implement" -> {"api","tasks","7","implement"}).
struct Route {
    const net::HttpRequest&  req;
    std::vector<std::string> seg;
    JsonValue                body;   ///< parsed JSON body (null when absent / not JSON)
    bool is(const char* method) const { return req.method == method; }
    std::string query(const char* k) const;
};

/// Each group answers the routes it owns, nullopt for the others.
std::optional<net::HttpResponse> project_routes(StudioDeps& d, const Route& r);
std::optional<net::HttpResponse> task_routes(StudioDeps& d, const Route& r);
std::optional<net::HttpResponse> source_routes(StudioDeps& d, const Route& r);

/// Cc project by code: the record, or the error response.
std::variant<JsonValue, net::HttpResponse> find_project(StudioDeps& d, const std::string& code);
std::string str_field(const JsonValue& v, const char* key);

} // namespace ergo::studio::plugins::studio
