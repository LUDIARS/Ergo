#pragma once

#include "ergo/network/types.h"
#include "ergo/network/socket.h"
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>

namespace ergo::network {

/// リアルタイム双方向通信チャネル
/// TCP 上で長さプレフィックス付きバイナリフレームを送受信する
/// Keep-Alive による接続維持とイベントコールバックを提供する
class StreamChannel {
public:
    StreamChannel();
    ~StreamChannel();

    StreamChannel(const StreamChannel&) = delete;
    StreamChannel& operator=(const StreamChannel&) = delete;

    /// 接続確立
    bool connect(const Address& addr, uint32_t timeout_ms = 5000);

    /// 切断
    void disconnect();

    /// フレーム送信
    bool send(const StreamFrame& frame);

    /// フレーム送信（バイト列）
    bool send(const void* data, size_t length);

    /// イベントコールバック登録
    void on_event(StreamCallback callback);

    /// 受信ループ開始（別スレッド）
    void start_receive();

    /// 受信ループ停止
    void stop_receive();

    /// 現在の状態取得
    StreamChannelState state() const { return state_.load(std::memory_order_acquire); }

    /// 接続中か
    bool is_connected() const { return state_.load(std::memory_order_acquire) == StreamChannelState::Connected; }

    /// 接続先アドレス取得
    const Address& remote_address() const { return remote_addr_; }

private:
    void receive_loop();
    bool send_frame_internal(const void* data, uint32_t length);
    bool recv_frame_internal(StreamFrame& frame);
    void emit_event(StreamEventType type, std::vector<uint8_t> data = {}, const std::string& msg = "");

    Socket                          socket_;
    Address                         remote_addr_;
    std::atomic<StreamChannelState> state_{StreamChannelState::Disconnected};

    StreamCallback                  callback_;
    std::mutex                      callback_mutex_;

    std::thread                     recv_thread_;
    std::atomic<bool>               recv_running_{false};
};

} // namespace ergo::network
