#include <gtest/gtest.h>
#include "ergo/network/connection_pool.h"
#include "ergo/network/socket.h"
#include <thread>
#include <chrono>

using namespace ergo::network;

class ConnectionPoolTest : public ::testing::Test {
protected:
    // ローカルリスナーを立てて接続テストを可能にする
    Socket listener_;
    uint16_t listen_port_ = 0;

    void SetUp() override {
        listener_.create(SocketProtocol::TCP);
        listener_.bind(0); // OS が空きポートを割り当て

        // バインドされたポートを取得
        struct sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        getsockname(listener_.handle(),
                    reinterpret_cast<struct sockaddr*>(&addr), &len);
        listen_port_ = ntohs(addr.sin_port);

        listener_.listen(8);
    }

    void TearDown() override {
        listener_.close();
    }

    Address local_addr() const {
        return {"127.0.0.1", listen_port_};
    }

    // リスナー側で接続を受け付けるスレッド
    void accept_connections(int count) {
        for (int i = 0; i < count; ++i) {
            Socket client = listener_.accept();
            // 受け付けたらすぐ閉じる（テスト用）
        }
    }
};

// 初期状態のプールは空であること
TEST_F(ConnectionPoolTest, InitialEmpty) {
    ConnectionPool pool;
    EXPECT_EQ(pool.total_connections(), 0u);
}

// 接続の取得と返却
TEST_F(ConnectionPoolTest, AcquireAndRelease) {
    ConnectionPool pool;
    auto addr = local_addr();

    // リスナー側で接続を受け付ける
    std::thread acceptor([this]() { accept_connections(1); });

    SocketHandle h = pool.acquire(addr);
    EXPECT_NE(h, kInvalidSocket);
    EXPECT_EQ(pool.total_connections(), 1u);
    EXPECT_EQ(pool.connections_for(addr), 1u);

    pool.release(h, addr);
    EXPECT_EQ(pool.total_connections(), 1u); // プールに残る

    acceptor.join();
}

// 返却された接続の再利用
TEST_F(ConnectionPoolTest, ReuseReleasedConnection) {
    ConnectionPool pool;
    auto addr = local_addr();

    std::thread acceptor([this]() { accept_connections(1); });

    SocketHandle h1 = pool.acquire(addr);
    pool.release(h1, addr);

    SocketHandle h2 = pool.acquire(addr);
    EXPECT_EQ(h1, h2); // 同じ接続が再利用される
    EXPECT_EQ(pool.total_connections(), 1u);

    pool.release(h2, addr);
    acceptor.join();
}

// discard で接続が破棄されること
TEST_F(ConnectionPoolTest, DiscardConnection) {
    ConnectionPool pool;
    auto addr = local_addr();

    std::thread acceptor([this]() { accept_connections(1); });

    SocketHandle h = pool.acquire(addr);
    pool.discard(h);
    EXPECT_EQ(pool.total_connections(), 0u);

    acceptor.join();
}

// ホスト別上限
TEST_F(ConnectionPoolTest, MaxPerHost) {
    KeepAliveConfig config;
    config.max_per_host = 2;
    config.max_connections = 10;
    ConnectionPool pool(config);
    auto addr = local_addr();

    // 3つの接続を同時に使用中にする
    std::thread acceptor([this]() { accept_connections(3); });

    SocketHandle h1 = pool.acquire(addr);
    SocketHandle h2 = pool.acquire(addr);
    // h1, h2 は使用中。3つ目を取得すると max_per_host=2 のため最古の未使用を回収するが
    // 全て使用中なので新規作成される
    SocketHandle h3 = pool.acquire(addr);

    EXPECT_NE(h1, kInvalidSocket);
    EXPECT_NE(h2, kInvalidSocket);
    EXPECT_NE(h3, kInvalidSocket);

    pool.discard(h1);
    pool.discard(h2);
    pool.discard(h3);
    acceptor.join();
}

// タイムアウトクリーンアップ
TEST_F(ConnectionPoolTest, CleanupTimedOut) {
    KeepAliveConfig config;
    config.timeout_seconds = 1; // 1秒でタイムアウト
    ConnectionPool pool(config);
    auto addr = local_addr();

    std::thread acceptor([this]() { accept_connections(1); });

    SocketHandle h = pool.acquire(addr);
    pool.release(h, addr);
    EXPECT_EQ(pool.total_connections(), 1u);

    // タイムアウトを待つ
    std::this_thread::sleep_for(std::chrono::seconds(2));

    size_t removed = pool.cleanup();
    EXPECT_GE(removed, 1u);
    EXPECT_EQ(pool.total_connections(), 0u);

    acceptor.join();
}

// 設定値の取得
TEST_F(ConnectionPoolTest, ConfigAccess) {
    KeepAliveConfig config;
    config.max_connections = 32;
    config.timeout_seconds = 120;
    ConnectionPool pool(config);

    EXPECT_EQ(pool.config().max_connections, 32u);
    EXPECT_EQ(pool.config().timeout_seconds, 120u);
}
