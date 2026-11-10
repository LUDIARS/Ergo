#include "client_set.h"

#include <vector>

namespace ergo::studio::plugins {

void ClientSet::add(std::shared_ptr<net::IWsConnection> conn, std::string role) {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint64_t id = conn->id();
    conns_[id] = {std::move(conn), std::move(role)};
}

void ClientSet::remove(uint64_t id) {
    std::lock_guard<std::mutex> lk(mtx_);
    conns_.erase(id);
}

void ClientSet::set_role(uint64_t id, std::string role) {
    std::lock_guard<std::mutex> lk(mtx_);
    const auto it = conns_.find(id);
    if (it != conns_.end()) it->second.role = std::move(role);
}

std::string ClientSet::role(uint64_t id) const {
    std::lock_guard<std::mutex> lk(mtx_);
    const auto it = conns_.find(id);
    return it == conns_.end() ? std::string() : it->second.role;
}

std::size_t ClientSet::size() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return conns_.size();
}

std::size_t ClientSet::count(const std::string& role) const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::size_t n = 0;
    for (const auto& [id, e] : conns_) {
        if (e.role == role) ++n;
    }
    return n;
}

void ClientSet::send(uint64_t id, const std::string& text) const {
    std::shared_ptr<net::IWsConnection> c;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it = conns_.find(id);
        if (it == conns_.end()) return;
        c = it->second.conn;
    }
    if (c->is_open()) c->send_text(text);
}

void ClientSet::broadcast(const std::string& text, const std::string& role) const {
    // Send outside the lock: a slow socket must not block add / remove.
    std::vector<std::shared_ptr<net::IWsConnection>> targets;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (const auto& [id, e] : conns_) {
            if (role.empty() || e.role == role) targets.push_back(e.conn);
        }
    }
    for (const auto& c : targets) {
        if (c->is_open()) c->send_text(text);
    }
}

} // namespace ergo::studio::plugins
