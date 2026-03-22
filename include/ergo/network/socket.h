#pragma once

#include "ergo/network/types.h"

namespace ergo::network {

/// 低レイヤソケット抽象化クラス
/// BSD Socket / WinSocket を統一インタフェースで提供する
class Socket {
public:
    Socket();
    explicit Socket(SocketHandle handle);
    ~Socket();

    // コピー禁止、ムーブ可
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    /// ソケット生成
    bool create(SocketProtocol protocol = SocketProtocol::TCP);

    /// 接続
    bool connect(const ResolvedAddress& addr);

    /// バインド
    bool bind(uint16_t port, const std::string& ip = "0.0.0.0");

    /// リッスン
    bool listen(int backlog = 128);

    /// 接続受付
    Socket accept();

    /// データ送信 (全データ送信を保証)
    int send(const void* data, size_t length);

    /// データ受信
    int recv(void* buffer, size_t length);

    /// ソケットクローズ
    void close();

    /// ノンブロッキング設定
    bool set_non_blocking(bool enabled);

    /// Keep-Alive 有効化 (OS レベル)
    bool set_keep_alive(bool enabled, int idle_seconds = 60);

    /// 送受信タイムアウト設定
    bool set_timeout(uint32_t send_ms, uint32_t recv_ms);

    /// ソケットハンドル取得
    SocketHandle handle() const { return handle_; }

    /// 有効なソケットか
    bool is_valid() const { return handle_ != kInvalidSocket; }

    /// 現在の状態
    SocketState state() const { return state_; }

    /// ハンドルの所有権を放棄して返す
    SocketHandle release();

private:
    SocketHandle handle_ = kInvalidSocket;
    SocketState  state_  = SocketState::Closed;
};

} // namespace ergo::network
