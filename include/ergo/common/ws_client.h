#pragma once

/// Shared WebSocket client transport (target `ergo_common_ws`). Bidirectional
/// (send + recv), reconnecting, single-thread worker. It is the one C++ WS
/// client in the tree: ergo_bind and the engine-side tool links (log /
/// particle / scene) all build their wire protocols on top of it instead of
/// writing their own socket code.
///
/// Threading: on_text / on_open run on the worker thread, never on the
/// caller's. A frame sent while disconnected is dropped, so a protocol that
/// needs state on the peer must re-send it from on_open (which fires after
/// every successful handshake, including reconnects). Inbound frames larger
/// than 1 MiB drop the connection, which is then retried.
///
/// Kept separate from `ergo_common` so the JSON codec stays socket-free.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ergo::common::ws {

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
    /// `leftover` receives any frame bytes that arrived with the 101 response.
    bool perform_handshake(int sock, std::string& leftover);
    bool drain_send_queue(int sock);
    /// Returns 1 when a full frame has been decoded into `payload`/`opcode`,
    /// 0 when more bytes are needed, -1 when the peer closed the stream or on
    /// a fatal protocol violation (oversized frame). The caller drops the
    /// connection on -1.
    int  read_one_frame(int sock, std::string& rx_buf, std::string& payload, uint8_t& opcode);

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

} // namespace ergo::common::ws
