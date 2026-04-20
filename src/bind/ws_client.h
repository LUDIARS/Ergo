#pragma once

/// Internal WebSocket client used by ergo_bind. Bidirectional (send + recv),
/// reconnecting, single-thread worker. Public-ish header so bind_engine.cpp
/// can include it; consumers should use the bind.h API instead.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ergo::bind::ws {

class Client {
public:
    using OnText = std::function<void(const std::string&)>;
    using OnOpen = std::function<void()>;

    Client();
    ~Client();

    Client(const Client&)            = delete;
    Client& operator=(const Client&) = delete;

    void set_on_text(OnText cb);   // called on the worker thread
    void set_on_open(OnOpen cb);   // called on the worker thread, after handshake

    void start(std::string host, uint16_t port, std::string path);
    void stop();

    bool is_connected() const { return connected_.load(std::memory_order_acquire); }

    /// Queue a text frame. Safe from any thread. Dropped if not connected
    /// (per design — sender must re-emit on reconnect via on_open).
    void send_text(std::string payload);

private:
    void worker_loop();
    bool connect_once();
    bool perform_handshake(int sock);
    bool drain_send_queue(int sock);
    bool read_one_frame(int sock, std::string& rx_buf, std::string& payload, uint8_t& opcode);

    std::string                     host_;
    uint16_t                        port_   = 0;
    std::string                     path_   = "/ws";

    std::atomic<bool>               running_   {false};
    std::atomic<bool>               stop_flag_ {false};
    std::atomic<bool>               connected_ {false};
    std::thread                     thread_;

    std::mutex                      cb_mtx_;
    OnText                          on_text_;
    OnOpen                          on_open_;

    std::mutex                      send_mtx_;
    std::deque<std::string>         send_queue_;
};

} // namespace ergo::bind::ws
