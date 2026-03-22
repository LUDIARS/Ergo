#include "ergo/network/connection_pool.h"
#include "ergo/network/dns_resolver.h"
#include <algorithm>

namespace ergo::network {

ConnectionPool::ConnectionPool(const KeepAliveConfig& config)
    : config_(config) {
}

ConnectionPool::~ConnectionPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [addr, conns] : pool_) {
        for (auto& conn : conns) {
            if (conn.socket != kInvalidSocket) {
#ifdef _WIN32
                ::closesocket(conn.socket);
#else
                ::close(conn.socket);
#endif
            }
        }
    }
    pool_.clear();
}

SocketHandle ConnectionPool::acquire(const Address& addr) {
    std::lock_guard<std::mutex> lock(mutex_);

    // プールから再利用可能な接続を探す
    auto it = pool_.find(addr);
    if (it != pool_.end()) {
        auto& conns = it->second;
        for (auto& conn : conns) {
            if (!conn.in_use && conn.state == SocketState::Connected) {
                conn.in_use = true;
                conn.last_used = std::chrono::steady_clock::now();
                return conn.socket;
            }
        }
    }

    // プール上限チェック
    size_t total = 0;
    for (const auto& [a, c] : pool_) {
        total += c.size();
    }
    if (total >= config_.max_connections) {
        evict_one();
    }

    // ホスト別上限チェック
    if (it != pool_.end() && it->second.size() >= config_.max_per_host) {
        // LRU: 最も古い未使用接続を回収
        auto& conns = it->second;
        auto oldest = std::find_if(conns.begin(), conns.end(),
                                   [](const PooledConnection& c) { return !c.in_use; });
        if (oldest != conns.end()) {
#ifdef _WIN32
            ::closesocket(oldest->socket);
#else
            ::close(oldest->socket);
#endif
            conns.erase(oldest);
        }
    }

    // 新規接続を作成
    SocketHandle handle = create_connection(addr);
    if (handle == kInvalidSocket) {
        return kInvalidSocket;
    }

    PooledConnection conn;
    conn.socket = handle;
    conn.address = addr;
    conn.state = SocketState::Connected;
    conn.last_used = std::chrono::steady_clock::now();
    conn.in_use = true;

    pool_[addr].push_back(std::move(conn));
    return handle;
}

void ConnectionPool::release(SocketHandle handle, const Address& addr) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pool_.find(addr);
    if (it == pool_.end()) return;

    for (auto& conn : it->second) {
        if (conn.socket == handle) {
            conn.in_use = false;
            conn.last_used = std::chrono::steady_clock::now();
            return;
        }
    }
}

void ConnectionPool::discard(SocketHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [addr, conns] : pool_) {
        for (auto it = conns.begin(); it != conns.end(); ++it) {
            if (it->socket == handle) {
#ifdef _WIN32
                ::closesocket(handle);
#else
                ::close(handle);
#endif
                conns.erase(it);
                return;
            }
        }
    }
}

size_t ConnectionPool::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    size_t removed = 0;

    for (auto& [addr, conns] : pool_) {
        auto it = conns.begin();
        while (it != conns.end()) {
            if (!it->in_use) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - it->last_used).count();
                if (elapsed >= static_cast<long long>(config_.timeout_seconds)) {
#ifdef _WIN32
                    ::closesocket(it->socket);
#else
                    ::close(it->socket);
#endif
                    it = conns.erase(it);
                    ++removed;
                    continue;
                }
            }
            ++it;
        }
    }

    // 空エントリの削除
    for (auto it = pool_.begin(); it != pool_.end();) {
        if (it->second.empty()) {
            it = pool_.erase(it);
        } else {
            ++it;
        }
    }

    return removed;
}

size_t ConnectionPool::total_connections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = 0;
    for (const auto& [addr, conns] : pool_) {
        total += conns.size();
    }
    return total;
}

size_t ConnectionPool::connections_for(const Address& addr) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pool_.find(addr);
    if (it == pool_.end()) return 0;
    return it->second.size();
}

SocketHandle ConnectionPool::create_connection(const Address& addr) {
    ResolvedAddress resolved;
    if (!DnsResolver::resolve_first(addr.host, addr.port, resolved)) {
        return kInvalidSocket;
    }

    SocketHandle handle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (handle == kInvalidSocket) {
        return kInvalidSocket;
    }

    // Keep-Alive 有効化
    if (config_.enabled) {
        int optval = 1;
        setsockopt(handle, SOL_SOCKET, SO_KEEPALIVE,
                   reinterpret_cast<const char*>(&optval), sizeof(optval));
    }

    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(resolved.port);
    inet_pton(AF_INET, resolved.ip.c_str(), &sa.sin_addr);

    if (::connect(handle, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == kSocketError) {
#ifdef _WIN32
        ::closesocket(handle);
#else
        ::close(handle);
#endif
        return kInvalidSocket;
    }

    return handle;
}

void ConnectionPool::evict_one() {
    // LRU: 全プールから最も古い未使用接続を回収
    std::chrono::steady_clock::time_point oldest_time = std::chrono::steady_clock::now();
    Address oldest_addr;
    size_t oldest_idx = 0;
    bool found = false;

    for (auto& [addr, conns] : pool_) {
        for (size_t i = 0; i < conns.size(); ++i) {
            if (!conns[i].in_use && conns[i].last_used < oldest_time) {
                oldest_time = conns[i].last_used;
                oldest_addr = addr;
                oldest_idx = i;
                found = true;
            }
        }
    }

    if (found) {
        auto& conns = pool_[oldest_addr];
#ifdef _WIN32
        ::closesocket(conns[oldest_idx].socket);
#else
        ::close(conns[oldest_idx].socket);
#endif
        conns.erase(conns.begin() + static_cast<ptrdiff_t>(oldest_idx));
        if (conns.empty()) {
            pool_.erase(oldest_addr);
        }
    }
}

} // namespace ergo::network
