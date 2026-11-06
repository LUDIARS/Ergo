#pragma once
/// A view's way to its plugin: the plugin's own HTTP API (IPluginHttp),
/// called in-process on a worker thread so the UI thread never waits on a
/// slow route (the studio plugin talks to remote services).
///
/// Views depend only on the routes, not on the plugin's classes (DIP), so the
/// same views work for external packs that speak the same API.
#include "ergo/common/json_min.h"
#include "ergo/studio/host/plugin.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ergo::studio::views {

using ergo::common::jsonm::JsonArray;
using ergo::common::jsonm::JsonValue;

struct ApiResult {
    int         status = 0;   ///< HTTP status; 0 = the route threw / no HTTP capability
    JsonValue   json;         ///< parsed body (null when not JSON)
    std::string body;         ///< raw body
    bool ok() const { return status >= 200 && status < 300; }
    /// "error" / "err" / "message" of the body, or "HTTP <status>".
    std::string error_text() const;
};

/// Build the request a route sees: "/<id><sub>" with the query split and
/// percent-decoded, a loopback Host and (for a body) application/json.
net::HttpRequest make_request(const std::string& plugin_id, const std::string& method, const std::string& sub_with_query,
                              const std::string& body);

/// Percent-encode one query value.
std::string url_encode(const std::string& v);

class PluginApi {
public:
    using Done = std::function<void(ApiResult)>;

    explicit PluginApi(host::IPlugin& plugin);
    ~PluginApi();
    PluginApi(const PluginApi&)            = delete;
    PluginApi& operator=(const PluginApi&) = delete;

    /// Queue a call; `done` runs on the worker thread (guard what it touches).
    void call(const std::string& method, const std::string& sub_with_query, const std::string& body, Done done);
    /// The same call on the caller's thread (tests, and the worker itself).
    ApiResult call_now(const std::string& method, const std::string& sub_with_query, const std::string& body);

private:
    struct Job {
        std::string method, path, body;
        Done        done;
    };
    void run();

    host::IPlugin&          plugin_;
    std::mutex              mtx_;
    std::condition_variable cv_;
    std::deque<Job>         jobs_;
    bool                    stop_ = false;
    std::thread             worker_;
};

} // namespace ergo::studio::views
