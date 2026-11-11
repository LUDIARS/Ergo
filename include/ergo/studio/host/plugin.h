#pragma once
/// Plugin contract of the native Ergo Studio (port of tools/ergo core/plugin.ts).
///
/// Each plugin owns a namespaced slice of the server:
///   HTTP: /<id>/*      (IPluginHttp, the "/<id>" prefix stripped)
///   WS:   /<id>/ws     (IPluginSocket)
/// and optionally reports health (IPluginHealth) and draws a tab (IPluginView,
/// declared by the shell). The capabilities are separate interfaces (ISP): a
/// plugin returns the ones it implements from IPlugin and nullptr otherwise.
#include "ergo/common/json_min.h"
#include "ergo/studio/net/server.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace ergo::studio::host {

using ergo::common::jsonm::JsonValue;

/// Static description shown by the shell picker and /api/plugins.
struct PluginInfo {
    std::string id;           ///< path segment, see is_valid_plugin_id()
    std::string title;
    std::string description;
    std::string icon;
    /// Optional one line the Studio logs under the plugin at start-up: how a
    /// client reaches it without the window (e.g. the WebSocket a game dials).
    /// "{addr}" stands for the server's <host>:<port>. "" = nothing to add.
    std::string usage_hint;
};

/// `hint` with every "{addr}" replaced by `addr` (<host>:<port>).
std::string expand_usage_hint(std::string_view hint, std::string_view addr);

/// Logging handed to every plugin (stdout / stderr of the Studio process).
using LogFn = net::LogFn;

class IPluginHttp {
public:
    virtual ~IPluginHttp() = default;
    /// `sub_path` is the request path without the "/<id>" prefix ("/" at least).
    /// Called on a connection thread; the plugin guards its own state.
    virtual net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) = 0;
};

class IPluginSocket {
public:
    virtual ~IPluginSocket() = default;
    /// A client opened /<id>/ws. Called on that connection's thread.
    virtual net::WsHandlers on_open(const net::HttpRequest& req, std::shared_ptr<net::IWsConnection> conn) = 0;
};

class IPluginHealth {
public:
    virtual ~IPluginHealth() = default;
    /// Object with a boolean "ok" plus free-form fields (clients, version, state...).
    virtual JsonValue snapshot() const = 0;
};

class IPluginView;   // shell side (ergo/studio/shell/plugin_view.h)

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual const PluginInfo& info() const = 0;
    virtual IPluginHttp*   http()   { return nullptr; }
    virtual IPluginSocket* socket() { return nullptr; }
    virtual IPluginHealth* health() { return nullptr; }
    virtual IPluginView*   view()   { return nullptr; }
};

/// Called once per Studio start. May throw; see instantiate_plugins().
using PluginFactory = std::function<std::unique_ptr<IPlugin>(const LogFn& log)>;

/// A factory from an external pack, with the pack entry it came from.
struct ExternalFactory {
    PluginFactory factory;
    std::string   source;
};

/// Path-safe plugin id: ^[a-z0-9][a-z0-9_-]*$
bool is_valid_plugin_id(const std::string& id);
/// Ids taken by the Studio's own routes ("api", "shell").
bool is_reserved_plugin_id(const std::string& id);

} // namespace ergo::studio::host
