#include "ergo/network/stream_channel.h"
#include "ergo/network/dns_resolver.h"
#include <cstring>

namespace ergo::network {

StreamChannel::StreamChannel() = default;

StreamChannel::~StreamChannel() {
    stop_receive();
    disconnect();
}

bool StreamChannel::connect(const Address& addr, uint32_t timeout_ms) {
    if (state_.load(std::memory_order_acquire) == StreamChannelState::Connected) {
        return true;
    }

    state_.store(StreamChannelState::Connecting, std::memory_order_release);
    remote_addr_ = addr;

    if (!socket_.create(SocketProtocol::TCP)) {
        state_.store(StreamChannelState::Error, std::memory_order_release);
        emit_event(StreamEventType::Error, {}, "Socket creation failed");
        return false;
    }

    socket_.set_timeout(timeout_ms, timeout_ms);
    socket_.set_keep_alive(true);

    ResolvedAddress resolved;
    if (!DnsResolver::resolve_first(addr.host, addr.port, resolved)) {
        state_.store(StreamChannelState::Error, std::memory_order_release);
        emit_event(StreamEventType::Error, {}, "DNS resolution failed");
        return false;
    }

    if (!socket_.connect(resolved)) {
        state_.store(StreamChannelState::Error, std::memory_order_release);
        emit_event(StreamEventType::Error, {}, "Connection failed");
        return false;
    }

    state_.store(StreamChannelState::Connected, std::memory_order_release);
    emit_event(StreamEventType::Connected);
    return true;
}

void StreamChannel::disconnect() {
    stop_receive();
    if (state_.load(std::memory_order_acquire) != StreamChannelState::Disconnected) {
        socket_.close();
        state_.store(StreamChannelState::Disconnected, std::memory_order_release);
        emit_event(StreamEventType::Disconnected);
    }
}

bool StreamChannel::send(const StreamFrame& frame) {
    return send(frame.data.data(), frame.data.size());
}

bool StreamChannel::send(const void* data, size_t length) {
    if (!is_connected()) return false;
    if (length > UINT32_MAX) return false;
    return send_frame_internal(data, static_cast<uint32_t>(length));
}

void StreamChannel::on_event(StreamCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_ = std::move(callback);
}

void StreamChannel::start_receive() {
    if (recv_running_.load(std::memory_order_acquire)) return;

    recv_running_.store(true, std::memory_order_release);
    recv_thread_ = std::thread([this]() { receive_loop(); });
}

void StreamChannel::stop_receive() {
    recv_running_.store(false, std::memory_order_release);
    if (recv_thread_.joinable()) {
        recv_thread_.join();
    }
}

void StreamChannel::receive_loop() {
    while (recv_running_.load(std::memory_order_acquire)) {
        if (!is_connected()) break;

        StreamFrame frame;
        if (recv_frame_internal(frame)) {
            emit_event(StreamEventType::DataReceived, std::move(frame.data));
        } else {
            if (recv_running_.load(std::memory_order_acquire)) {
                // 受信エラー = 切断
                state_.store(StreamChannelState::Disconnected, std::memory_order_release);
                emit_event(StreamEventType::Disconnected, {}, "Connection lost");
                break;
            }
        }
    }
}

bool StreamChannel::send_frame_internal(const void* data, uint32_t length) {
    // フレームフォーマット: [4バイト長さ (ネットワークバイトオーダー)][データ]
    uint32_t net_length = htonl(length);
    int sent = socket_.send(&net_length, sizeof(net_length));
    if (sent != sizeof(net_length)) return false;

    if (length > 0) {
        sent = socket_.send(data, length);
        if (sent != static_cast<int>(length)) return false;
    }
    return true;
}

bool StreamChannel::recv_frame_internal(StreamFrame& frame) {
    // 長さプレフィックス読み取り
    uint32_t net_length = 0;
    int received = socket_.recv(&net_length, sizeof(net_length));
    if (received != sizeof(net_length)) return false;

    uint32_t length = ntohl(net_length);
    if (length == 0) {
        frame.data.clear();
        return true;
    }

    // データ読み取り
    frame.data.resize(length);
    size_t total_read = 0;
    while (total_read < length) {
        int r = socket_.recv(frame.data.data() + total_read, length - total_read);
        if (r <= 0) return false;
        total_read += static_cast<size_t>(r);
    }

    return true;
}

void StreamChannel::emit_event(StreamEventType type, std::vector<uint8_t> data,
                                const std::string& msg) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        StreamEvent event;
        event.type = type;
        event.data = std::move(data);
        event.message = msg;
        callback_(event);
    }
}

} // namespace ergo::network
