#include <gtest/gtest.h>
#include "ergo/network/stream_channel.h"
#include "ergo/network/socket.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>

using namespace ergo::network;

class StreamChannelTest : public ::testing::Test {
protected:
    Socket listener_;
    uint16_t listen_port_ = 0;

    void SetUp() override {
        listener_.create(SocketProtocol::TCP);
        listener_.bind(0);

        struct sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        getsockname(listener_.handle(),
                    reinterpret_cast<struct sockaddr*>(&addr), &len);
        listen_port_ = ntohs(addr.sin_port);

        listener_.listen(4);
    }

    void TearDown() override {
        listener_.close();
    }

    Address local_addr() const {
        return {"127.0.0.1", listen_port_};
    }
};

// 初期状態
TEST_F(StreamChannelTest, InitialState) {
    StreamChannel channel;
    EXPECT_EQ(channel.state(), StreamChannelState::Disconnected);
    EXPECT_FALSE(channel.is_connected());
}

// 接続確立と切断
TEST_F(StreamChannelTest, ConnectAndDisconnect) {
    std::atomic<bool> connected{false};
    std::atomic<bool> disconnected{false};

    StreamChannel channel;
    channel.on_event([&](const StreamEvent& event) {
        if (event.type == StreamEventType::Connected) connected = true;
        if (event.type == StreamEventType::Disconnected) disconnected = true;
    });

    std::thread acceptor([this]() {
        Socket client = listener_.accept();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        client.close();
    });

    EXPECT_TRUE(channel.connect(local_addr()));
    EXPECT_TRUE(channel.is_connected());
    EXPECT_TRUE(connected.load());

    channel.disconnect();
    EXPECT_EQ(channel.state(), StreamChannelState::Disconnected);
    EXPECT_TRUE(disconnected.load());

    acceptor.join();
}

// フレーム送受信
TEST_F(StreamChannelTest, SendAndReceiveFrame) {
    std::vector<uint8_t> received_data;
    std::atomic<bool> data_received{false};

    StreamChannel channel;
    channel.on_event([&](const StreamEvent& event) {
        if (event.type == StreamEventType::DataReceived) {
            received_data = event.data;
            data_received = true;
        }
    });

    std::thread server([this]() {
        Socket client = listener_.accept();
        if (!client.is_valid()) return;

        // フレームを受信して同じデータをエコーバック
        uint32_t net_len = 0;
        client.recv(&net_len, sizeof(net_len));
        uint32_t len = ntohl(net_len);

        std::vector<uint8_t> buf(len);
        size_t total = 0;
        while (total < len) {
            int r = client.recv(buf.data() + total, len - total);
            if (r <= 0) break;
            total += r;
        }

        // エコーバック
        net_len = htonl(len);
        client.send(&net_len, sizeof(net_len));
        client.send(buf.data(), len);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        client.close();
    });

    EXPECT_TRUE(channel.connect(local_addr()));

    // 送信
    std::string msg = "Hello, Stream!";
    EXPECT_TRUE(channel.send(msg.c_str(), msg.size()));

    // 受信ループ開始
    channel.start_receive();

    // データ受信を待機
    auto start = std::chrono::steady_clock::now();
    while (!data_received.load() &&
           std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    channel.stop_receive();
    channel.disconnect();

    EXPECT_TRUE(data_received.load());
    std::string received_str(received_data.begin(), received_data.end());
    EXPECT_EQ(received_str, "Hello, Stream!");

    server.join();
}

// 空フレームの送受信
TEST_F(StreamChannelTest, EmptyFrame) {
    std::atomic<bool> data_received{false};

    StreamChannel channel;
    channel.on_event([&](const StreamEvent& event) {
        if (event.type == StreamEventType::DataReceived) {
            data_received = true;
        }
    });

    std::thread server([this]() {
        Socket client = listener_.accept();
        if (!client.is_valid()) return;

        // 空フレームを受信
        uint32_t net_len = 0;
        client.recv(&net_len, sizeof(net_len));

        // 空フレームをエコーバック
        net_len = htonl(0);
        client.send(&net_len, sizeof(net_len));

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        client.close();
    });

    EXPECT_TRUE(channel.connect(local_addr()));
    EXPECT_TRUE(channel.send(nullptr, 0));

    channel.start_receive();

    auto start = std::chrono::steady_clock::now();
    while (!data_received.load() &&
           std::chrono::steady_clock::now() - start < std::chrono::seconds(3)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    channel.stop_receive();
    channel.disconnect();
    EXPECT_TRUE(data_received.load());

    server.join();
}

// 接続失敗時のエラーイベント
TEST_F(StreamChannelTest, ConnectionFailure) {
    std::atomic<bool> error_received{false};

    StreamChannel channel;
    channel.on_event([&](const StreamEvent& event) {
        if (event.type == StreamEventType::Error) {
            error_received = true;
        }
    });

    // 存在しないポートへの接続
    Address bad_addr{"127.0.0.1", 1};
    EXPECT_FALSE(channel.connect(bad_addr));
    EXPECT_TRUE(error_received.load());
}

// 未接続時の送信
TEST_F(StreamChannelTest, SendWhileDisconnected) {
    StreamChannel channel;
    EXPECT_FALSE(channel.send("test", 4));
}
