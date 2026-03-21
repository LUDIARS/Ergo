#pragma once

#include "ergo/network/types.h"
#include "ergo/network/socket.h"
#include <mutex>
#include <vector>
#include <memory>

namespace ergo::network {

/// Keep-Alive 接続プール
/// ホスト:ポート単位で TCP 接続を保持・再利用する
class ConnectionPool {
public:
    explicit ConnectionPool(const KeepAliveConfig& config = KeepAliveConfig{});
    ~ConnectionPool();

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    /// プールから接続を取得（なければ新規作成）
    /// 戻り値: ソケットハンドル。失敗時は kInvalidSocket
    SocketHandle acquire(const Address& addr);

    /// 使用済み接続をプールに返却
    void release(SocketHandle handle, const Address& addr);

    /// 接続を破棄（再利用しない場合）
    void discard(SocketHandle handle);

    /// タイムアウトした接続をクリーンアップ
    size_t cleanup();

    /// プール内の全接続数
    size_t total_connections() const;

    /// 指定ホストの接続数
    size_t connections_for(const Address& addr) const;

    /// プール設定を取得
    const KeepAliveConfig& config() const { return config_; }

private:
    SocketHandle create_connection(const Address& addr);
    void evict_one();

    KeepAliveConfig config_;
    mutable std::mutex mutex_;
    std::unordered_map<Address, std::vector<PooledConnection>, AddressHash> pool_;
};

} // namespace ergo::network
