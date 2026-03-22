#pragma once

#include "ergo/network/types.h"
#include "ergo/network/connection_pool.h"
#include "ergo/network/http_client.h"
#include "ergo/network/stream_channel.h"
#include <memory>
#include <unordered_map>

namespace ergo::network {

/// ネットワークシステム ファサードクラス
/// 全ネットワーク機能の初期化・管理・終了を統括する
class NetworkSystem {
public:
    NetworkSystem();
    ~NetworkSystem();

    NetworkSystem(const NetworkSystem&) = delete;
    NetworkSystem& operator=(const NetworkSystem&) = delete;

    /// プラットフォーム初期化（WinSock 初期化など）
    bool initialize(const NetworkConfig& config = NetworkConfig{});

    /// シャットダウン
    void shutdown();

    /// 初期化済みか
    bool is_initialized() const { return initialized_; }

    /// 接続プール取得
    ConnectionPool& pool() { return *pool_; }

    /// HTTP クライアント取得
    HttpClient& http() { return *http_client_; }

    /// ストリームチャネル生成
    std::shared_ptr<StreamChannel> create_stream();

    /// 接続プールのクリーンアップ実行
    size_t cleanup_pools();

private:
    bool                                          initialized_ = false;
    NetworkConfig                                 config_;
    std::unique_ptr<ConnectionPool>               pool_;
    std::unique_ptr<HttpClient>                   http_client_;
    std::vector<std::shared_ptr<StreamChannel>>   streams_;
};

} // namespace ergo::network
