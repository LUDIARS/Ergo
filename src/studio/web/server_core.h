#pragma once
/// What every listener and connection of one WebServer shares. Lives as long
/// as the server; the event loop thread owns `sessions`.
#include "ergo/studio/web/request_pipeline.h"
#include "ergo/studio/web/web_server.h"
#include "ws_session.h"

#include <portus/event_loop.hpp>
#include <portus/log.hpp>
#include <portus/thread_pool.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace ergo::studio::web {

struct ServerCore {
    ServerCore(portus::EventLoop& l, RequestPipeline p, WebLimits lim, portus::Logger lg, net::LogFn plain)
        : loop(l), pipeline(std::move(p)), limits(lim), log(std::move(lg)), plain_log(std::move(plain)),
          pool(std::make_unique<portus::ThreadPool>(lim.worker_threads, lim.max_pending)) {}

    portus::EventLoop&                       loop;
    RequestPipeline                          pipeline;
    WebLimits                                limits;
    portus::Logger                           log;
    net::LogFn                               plain_log;
    std::unique_ptr<portus::ThreadPool>      pool;
    std::atomic<bool>                        closed{false};
    std::atomic<uint64_t>                    next_ws_id{1};
    std::vector<std::unique_ptr<WsSession>>  sessions;   // loop thread only
    std::vector<std::weak_ptr<portus::WebSocket>> sockets;   // loop thread only (terminated on stop)

    /// Drop the sessions whose close has run and the sockets that are gone (loop thread).
    void reap_sessions();
};

} // namespace ergo::studio::web
