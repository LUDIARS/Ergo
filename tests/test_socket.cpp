#include <gtest/gtest.h>
#include "ergo/network/socket.h"

using namespace ergo::network;

class SocketTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ソケットの生成・クローズが正しく動作すること
TEST_F(SocketTest, CreateAndClose) {
    Socket sock;
    EXPECT_FALSE(sock.is_valid());
    EXPECT_EQ(sock.state(), SocketState::Closed);

    EXPECT_TRUE(sock.create(SocketProtocol::TCP));
    EXPECT_TRUE(sock.is_valid());

    sock.close();
    EXPECT_FALSE(sock.is_valid());
    EXPECT_EQ(sock.state(), SocketState::Closed);
}

// UDP ソケットも生成できること
TEST_F(SocketTest, CreateUDP) {
    Socket sock;
    EXPECT_TRUE(sock.create(SocketProtocol::UDP));
    EXPECT_TRUE(sock.is_valid());
}

// ムーブコンストラクタが正しく動作すること
TEST_F(SocketTest, MoveConstructor) {
    Socket sock;
    sock.create(SocketProtocol::TCP);
    SocketHandle original_handle = sock.handle();
    EXPECT_TRUE(sock.is_valid());

    Socket moved(std::move(sock));
    EXPECT_TRUE(moved.is_valid());
    EXPECT_EQ(moved.handle(), original_handle);
    EXPECT_FALSE(sock.is_valid());
}

// ムーブ代入が正しく動作すること
TEST_F(SocketTest, MoveAssignment) {
    Socket sock1;
    sock1.create(SocketProtocol::TCP);
    SocketHandle h1 = sock1.handle();

    Socket sock2;
    sock2 = std::move(sock1);
    EXPECT_TRUE(sock2.is_valid());
    EXPECT_EQ(sock2.handle(), h1);
    EXPECT_FALSE(sock1.is_valid());
}

// release でハンドルの所有権が移転されること
TEST_F(SocketTest, Release) {
    Socket sock;
    sock.create(SocketProtocol::TCP);
    SocketHandle h = sock.release();
    EXPECT_NE(h, kInvalidSocket);
    EXPECT_FALSE(sock.is_valid());

    // 手動でクローズ
#ifdef _WIN32
    ::closesocket(h);
#else
    ::close(h);
#endif
}

// ノンブロッキング設定
TEST_F(SocketTest, NonBlocking) {
    Socket sock;
    sock.create(SocketProtocol::TCP);
    EXPECT_TRUE(sock.set_non_blocking(true));
    EXPECT_TRUE(sock.set_non_blocking(false));
}

// Keep-Alive 設定
TEST_F(SocketTest, KeepAlive) {
    Socket sock;
    sock.create(SocketProtocol::TCP);
    EXPECT_TRUE(sock.set_keep_alive(true, 30));
    EXPECT_TRUE(sock.set_keep_alive(false));
}

// タイムアウト設定
TEST_F(SocketTest, Timeout) {
    Socket sock;
    sock.create(SocketProtocol::TCP);
    EXPECT_TRUE(sock.set_timeout(5000, 5000));
}

// バインドとリッスン
TEST_F(SocketTest, BindAndListen) {
    Socket sock;
    sock.create(SocketProtocol::TCP);
    EXPECT_TRUE(sock.bind(0)); // OS が空きポートを割り当て
    EXPECT_TRUE(sock.listen(5));
    EXPECT_EQ(sock.state(), SocketState::Listening);
}

// 無効なソケットへの操作
TEST_F(SocketTest, InvalidSocketOperations) {
    Socket sock;
    EXPECT_FALSE(sock.set_non_blocking(true));
    EXPECT_FALSE(sock.set_keep_alive(true));
    EXPECT_FALSE(sock.set_timeout(1000, 1000));
    EXPECT_EQ(sock.send("test", 4), -1);

    char buf[16];
    EXPECT_EQ(sock.recv(buf, sizeof(buf)), -1);
}
