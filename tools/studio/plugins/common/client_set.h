#pragma once
/// Thread-safe set of open WebSocket connections of one plugin, each with a
/// role tag ("engine" / "ui" / ...). Plugins broadcast through it instead of
/// keeping their own lists.
#include "ergo/studio/net/server.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace ergo::studio::plugins {

class ClientSet {
public:
    void add(std::shared_ptr<net::IWsConnection> conn, std::string role = {});
    void remove(uint64_t id);
    void set_role(uint64_t id, std::string role);
    /// Role of `id`, or "" when unknown / untagged.
    std::string role(uint64_t id) const;

    std::size_t size() const;
    std::size_t count(const std::string& role) const;

    void send(uint64_t id, const std::string& text) const;
    /// To every connection, or only those whose role equals `role` when set.
    void broadcast(const std::string& text, const std::string& role = {}) const;

private:
    struct Entry {
        std::shared_ptr<net::IWsConnection> conn;
        std::string                         role;
    };
    mutable std::mutex           mtx_;
    std::map<uint64_t, Entry>    conns_;
};

} // namespace ergo::studio::plugins
