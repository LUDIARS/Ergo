#include <gtest/gtest.h>
#include "ergo/network/network_system.h"

using namespace ergo::network;

class NetworkSystemTest : public ::testing::Test {};

// 初期化とシャットダウン
TEST_F(NetworkSystemTest, InitializeAndShutdown) {
    NetworkSystem system;
    EXPECT_FALSE(system.is_initialized());

    EXPECT_TRUE(system.initialize());
    EXPECT_TRUE(system.is_initialized());

    system.shutdown();
    EXPECT_FALSE(system.is_initialized());
}

// 二重初期化
TEST_F(NetworkSystemTest, DoubleInitialize) {
    NetworkSystem system;
    EXPECT_TRUE(system.initialize());
    EXPECT_TRUE(system.initialize()); // 2回目もtrue（既に初期化済み）
    system.shutdown();
}

// カスタム設定での初期化
TEST_F(NetworkSystemTest, CustomConfig) {
    NetworkSystem system;
    NetworkConfig config;
    config.keep_alive.max_connections = 32;
    config.keep_alive.timeout_seconds = 120;
    config.recv_buffer_size = 16384;

    EXPECT_TRUE(system.initialize(config));
    EXPECT_EQ(system.pool().config().max_connections, 32u);
    EXPECT_EQ(system.pool().config().timeout_seconds, 120u);
    system.shutdown();
}

// ストリームチャネルの生成
TEST_F(NetworkSystemTest, CreateStream) {
    NetworkSystem system;
    system.initialize();

    auto stream = system.create_stream();
    EXPECT_NE(stream, nullptr);
    EXPECT_EQ(stream->state(), StreamChannelState::Disconnected);

    system.shutdown();
}

// クリーンアップ
TEST_F(NetworkSystemTest, CleanupPools) {
    NetworkSystem system;
    system.initialize();

    size_t removed = system.cleanup_pools();
    EXPECT_EQ(removed, 0u); // 接続がないので0

    system.shutdown();
}

// シャットダウン後のクリーンアップ
TEST_F(NetworkSystemTest, CleanupAfterShutdown) {
    NetworkSystem system;
    // 初期化していない状態
    size_t removed = system.cleanup_pools();
    EXPECT_EQ(removed, 0u);
}

// プラットフォーム間でAPIが統一されていること（コンパイルテスト）
TEST_F(NetworkSystemTest, UnifiedAPI) {
    NetworkSystem system;
    system.initialize();

    // 全APIが呼び出し可能であることを確認
    auto& pool = system.pool();
    auto& http = system.http();
    auto stream = system.create_stream();

    (void)pool;
    (void)http;
    EXPECT_NE(stream, nullptr);

    system.shutdown();
}
