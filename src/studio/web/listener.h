#pragma once
/// One listening socket = one gate. Accepts connections, keeps them until they
/// close (or become WebSockets) and closes the ones past their deadline.
/// open / shutdown on the loop thread (or before the loop runs).
#include "connection.h"

#include <portus/socket.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace ergo::studio::web {

class Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(ServerCore& core, net::Gate gate) : core_(core), gate_(gate) {}

    /// Start watching an already bound socket (portus::listen_tcp).
    void watch(portus::Listener bound);
    void shutdown();

private:
    void on_accept();
    void sweep();

    ServerCore&                                             core_;
    net::Gate                                               gate_;
    portus::socket_t                                        fd_ = portus::kInvalidSocket;
    std::uint64_t                                           sweep_timer_ = 0;
    std::uint64_t                                           next_id_     = 1;
    std::unordered_map<std::uint64_t, std::shared_ptr<Connection>> conns_;
};

} // namespace ergo::studio::web
