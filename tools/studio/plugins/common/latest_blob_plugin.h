#pragma once
/// A plugin that keeps the last JSON document an engine published and hands
/// it to every client (profile: Chrome traces, pictor_perf: perf snapshots).
///   WS   in : {op:"publish", <field>:"<json string>"} | {op:"ping"}
///   WS   out: {op:<frame_op>, <field>:string|null, clients} | {op:"ack"}
///   HTTP    : GET /api/health, and when http_publish is set
///             POST /api/publish (body = document), GET /api/snapshot
#include "client_set.h"
#include "json_util.h"

#include "ergo/studio/host/plugin.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>

namespace ergo::studio::plugins {

struct LatestBlobSpec {
    host::PluginInfo info;
    std::string      frame_op;       ///< "trace" / "snapshot"
    std::string      field;          ///< "trace" / "snapshot"
    std::string      has_key;        ///< health flag: "has_trace" / "has_snapshot"
    int              schema_version = 1;
    bool             http_publish   = false;
    /// Empty = accept any string. Otherwise false rejects the document
    /// (logged, never stored).
    std::function<bool(const std::string&)> validate;
    std::string      invalid_error;  ///< HTTP 400 message for a rejected document
};

class LatestBlobPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginSocket,
                               public host::IPluginHealth {
public:
    LatestBlobPlugin(LatestBlobSpec spec, host::LogFn log);

    const host::PluginInfo& info() const override { return spec_.info; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginSocket*    socket() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    net::WsHandlers   on_open(const net::HttpRequest& req, std::shared_ptr<net::IWsConnection> conn) override;
    JsonValue         snapshot() const override;

    // ---- In-process API ----
    std::optional<std::string> current() const;
    /// Store and broadcast. False (and logged) when validate() rejects it.
    bool                       publish(const std::string& doc);
    uint64_t                   revision() const { return revision_.load(); }

private:
    std::string frame() const;

    LatestBlobSpec             spec_;
    host::LogFn                log_;
    ClientSet                  clients_;
    mutable std::mutex         mtx_;
    std::optional<std::string> current_;
    std::atomic<uint64_t>      revision_{0};
};

} // namespace ergo::studio::plugins
