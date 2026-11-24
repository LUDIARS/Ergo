#include "ergo/studio/web/web_server.h"

#include "listener.h"
#include "log_bridge.h"
#include "server_core.h"

#include <portus/event_loop.hpp>
#include <portus/socket.hpp>

#include <algorithm>
#include <exception>
#include <utility>

namespace ergo::studio::web {
namespace {

const char* gate_name(net::Gate g) { return g == net::Gate::local ? "local" : "remote"; }

/// The access record: who reached what. Headers and query strings are not
/// written (they may carry tokens); Portus masks secret field names besides.
AccessLogFn access_log(const portus::Logger* log) {
    return [log](const net::HttpRequest& req, int status, const std::string& note) {
        portus::Json fields = portus::Json::object({{"method", req.method},
                                                     {"path", req.path},
                                                     {"status", status},
                                                     {"gate", gate_name(req.gate)}});
        if (!req.who.empty()) fields.set("who", req.who);
        if (!note.empty()) fields.set("note", note);
        if (status >= 400) log->warn("refused", fields);
        else if (req.gate == net::Gate::remote) log->info("remote", fields);
        else log->debug("local", fields);
    };
}

} // namespace

void ServerCore::reap_sessions() {
    sessions.erase(std::remove_if(sessions.begin(), sessions.end(),
                                  [](const std::unique_ptr<WsSession>& s) { return s->finished(); }),
                   sessions.end());
    sockets.erase(std::remove_if(sockets.begin(), sockets.end(),
                                 [](const std::weak_ptr<portus::WebSocket>& w) { return w.expired(); }),
                  sockets.end());
}

struct WebServer::Impl {
    Impl(WebServerConfig c, net::IHttpHandler& http, net::IWsAcceptor& ws, std::vector<std::string> plugin_ids,
         net::LogFn plain)
        : cfg(std::move(c)), log(studio_logger(plain)),
          core(std::make_unique<ServerCore>(
              loop_thread.loop(),
              RequestPipeline(ingress::EntrancePolicy{cfg.bind_host, cfg.public_hosts, cfg.tailnet_hosts},
                              RouteTable(std::move(plugin_ids)), http, ws, access_log(&log)),
              cfg.limits, log, std::move(plain))) {}

    WebServerConfig                        cfg;
    portus::Logger                         log;
    portus::LoopThread                     loop_thread;
    std::unique_ptr<ServerCore>            core;   // after the loop: destroyed before it
    std::vector<std::shared_ptr<Listener>> listeners;
    bool                                   running = false;
};

WebServer::WebServer(WebServerConfig cfg, net::IHttpHandler& http, net::IWsAcceptor& ws,
                     std::vector<std::string> plugin_ids, net::LogFn log)
    : impl_(std::make_unique<Impl>(std::move(cfg), http, ws, std::move(plugin_ids), std::move(log))) {}

WebServer::~WebServer() { stop(); }

bool WebServer::start(std::string& err) {
    if (impl_->running) return true;
    Impl& im = *impl_;
    std::vector<std::pair<net::Gate, portus::Listener>> bound;
    try {
        bound.emplace_back(net::Gate::local, portus::listen_tcp(im.cfg.bind_host, im.cfg.local_port));
        if (im.cfg.remote_port) {
            bound.emplace_back(net::Gate::remote, portus::listen_tcp(im.cfg.bind_host, *im.cfg.remote_port));
        }
    } catch (const std::exception& e) {
        for (const auto& [gate, l] : bound) portus::close_socket(l.fd);
        err = std::string("cannot listen on ") + im.cfg.bind_host + ": " + e.what();
        return false;
    }
    local_port_ = static_cast<uint16_t>(bound.front().second.port);
    remote_port_.reset();
    if (bound.size() > 1) remote_port_ = static_cast<uint16_t>(bound.back().second.port);
    im.core->closed = false;
    im.core->loop.dispatch_sync([&im, &bound] {
        for (const auto& [gate, l] : bound) {
            auto listener = std::make_shared<Listener>(*im.core, gate);
            listener->watch(l);
            im.listeners.push_back(std::move(listener));
        }
    });
    im.running = true;
    im.core->log.info("listening", portus::Json::object({{"local", local_port_},
                                                         {"remote", remote_port_ ? portus::Json(*remote_port_)
                                                                                 : portus::Json(nullptr)}}));
    return true;
}

void WebServer::stop() {
    Impl& im = *impl_;
    if (!im.running) return;
    im.running       = false;
    im.core->closed  = true;
    im.core->loop.dispatch_sync([&im] {
        for (const auto& l : im.listeners) l->shutdown();
        im.listeners.clear();
        std::vector<std::weak_ptr<portus::WebSocket>> sockets;
        sockets.swap(im.core->sockets);
        for (const auto& w : sockets) {
            if (auto ws = w.lock()) ws->terminate();   // runs on_close: each session queues its close
        }
    });
    // Running handlers finish (their answers are dropped), then every session's close runs.
    im.core->pool = std::make_unique<portus::ThreadPool>(im.cfg.limits.worker_threads, im.cfg.limits.max_pending);
    std::vector<std::unique_ptr<WsSession>> sessions;
    im.core->loop.dispatch_sync([&im, &sessions] { sessions.swap(im.core->sessions); });
    for (auto& s : sessions) s->end();
    sessions.clear();   // joins each session thread
}

} // namespace ergo::studio::web
