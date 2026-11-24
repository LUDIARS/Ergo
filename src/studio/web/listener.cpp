#include "listener.h"

#include <vector>

namespace ergo::studio::web {
namespace {

constexpr int kSweepMs = 250;

} // namespace

void Listener::watch(portus::Listener bound) {
    fd_ = bound.fd;
    std::weak_ptr<Listener> weak = shared_from_this();
    core_.loop.add(fd_, portus::kRead, [weak](unsigned) {
        if (auto self = weak.lock()) self->on_accept();
    });
    sweep_timer_ = core_.loop.set_timer(kSweepMs, [weak] {
        if (auto self = weak.lock()) self->sweep();
    }, true);
}

void Listener::on_accept() {
    for (;;) {
        const portus::socket_t fd = portus::accept_tcp(fd_, nullptr);
        if (fd == portus::kInvalidSocket) return;   // nothing waiting (or a transient error: the next event retries)
        if (conns_.size() >= core_.limits.max_connections) {
            portus::close_socket(fd);
            continue;
        }
        const std::uint64_t     id   = next_id_++;
        std::weak_ptr<Listener> weak = shared_from_this();
        auto tcp  = std::make_shared<portus::TcpConn>(core_.loop, fd, static_cast<std::size_t>(-1));
        auto conn = std::make_shared<Connection>(core_, gate_, std::move(tcp), [weak, id] {
            if (auto self = weak.lock()) self->conns_.erase(id);
        });
        conns_.emplace(id, conn);
        conn->start();
    }
}

void Listener::sweep() {
    const auto now = Connection::Clock::now();
    std::vector<std::shared_ptr<Connection>> expired;
    for (const auto& [id, conn] : conns_) {
        if (conn->expired(now)) expired.push_back(conn);
    }
    for (const auto& conn : expired) conn->close();
}

void Listener::shutdown() {
    if (fd_ != portus::kInvalidSocket) {
        core_.loop.remove(fd_);
        portus::close_socket(fd_);
        fd_ = portus::kInvalidSocket;
    }
    if (sweep_timer_ != 0) core_.loop.cancel_timer(sweep_timer_);
    sweep_timer_ = 0;
    std::vector<std::shared_ptr<Connection>> all;
    for (const auto& [id, conn] : conns_) all.push_back(conn);
    for (const auto& conn : all) conn->close();
    conns_.clear();
}

} // namespace ergo::studio::web
