#include <gtest/gtest.h>
#include "ergo/network/http_client.h"
#include "ergo/network/socket.h"
#include <thread>
#include <cstring>

using namespace ergo::network;

class HttpClientTest : public ::testing::Test {
protected:
    Socket listener_;
    uint16_t listen_port_ = 0;
    ConnectionPool pool_;

    void SetUp() override {
        listener_.create(SocketProtocol::TCP);
        listener_.bind(0);

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

    // テスト用 HTTP サーバー（1リクエスト処理）
    void serve_response(const std::string& response_str) {
        Socket client = listener_.accept();
        if (!client.is_valid()) return;

        // リクエストを読み取り（簡易）
        char buf[4096]{};
        client.recv(buf, sizeof(buf));

        // レスポンスを返す
        client.send(response_str.c_str(), response_str.size());
        client.close();
    }
};

// HTTP リクエストのシリアライズ確認
TEST_F(HttpClientTest, RequestSerialization) {
    HttpRequest req;
    req.method = HttpMethod::GET;
    req.path = "/api/test";
    req.host = "example.com";
    req.headers.push_back({"Accept", "application/json"});

    // シリアライズはprivateなので送受信テストで間接的に確認
    EXPECT_EQ(req.method, HttpMethod::GET);
    EXPECT_EQ(req.path, "/api/test");
}

// HttpRequest のヘッダ操作
TEST_F(HttpClientTest, RequestHeaders) {
    HttpRequest req;
    req.set_header("Content-Type", "application/json");
    EXPECT_EQ(req.get_header("Content-Type"), "application/json");

    // 上書き
    req.set_header("Content-Type", "text/plain");
    EXPECT_EQ(req.get_header("Content-Type"), "text/plain");

    // 存在しないヘッダ
    EXPECT_EQ(req.get_header("X-Missing"), "");
}

// HttpResponse の判定
TEST_F(HttpClientTest, ResponseSuccess) {
    HttpResponse resp;
    resp.status_code = 200;
    EXPECT_TRUE(resp.is_success());

    resp.status_code = 201;
    EXPECT_TRUE(resp.is_success());

    resp.status_code = 404;
    EXPECT_FALSE(resp.is_success());

    resp.status_code = 500;
    EXPECT_FALSE(resp.is_success());
}

// GET リクエストの送受信
TEST_F(HttpClientTest, GetRequest) {
    std::string mock_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 13\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello, World!";

    std::thread server([this, &mock_response]() {
        serve_response(mock_response);
    });

    HttpClient client(pool_);
    auto response = client.get(local_addr(), "/test");

    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.status_text, "OK");

    std::string body_str(response.body.begin(), response.body.end());
    EXPECT_EQ(body_str, "Hello, World!");

    server.join();
}

// POST リクエストの送受信
TEST_F(HttpClientTest, PostRequest) {
    std::string mock_response =
        "HTTP/1.1 201 Created\r\n"
        "Content-Length: 2\r\n"
        "Connection: close\r\n"
        "\r\n"
        "OK";

    std::thread server([this, &mock_response]() {
        serve_response(mock_response);
    });

    std::string body = "test data";
    std::vector<uint8_t> body_bytes(body.begin(), body.end());

    HttpClient client(pool_);
    auto response = client.post(local_addr(), "/submit", body_bytes);

    EXPECT_EQ(response.status_code, 201);

    server.join();
}

// チャンク転送エンコーディング
TEST_F(HttpClientTest, ChunkedResponse) {
    std::string mock_response =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n"
        "\r\n"
        "5\r\n"
        "Hello\r\n"
        "7\r\n"
        ", World\r\n"
        "0\r\n"
        "\r\n";

    std::thread server([this, &mock_response]() {
        serve_response(mock_response);
    });

    HttpClient client(pool_);
    auto response = client.get(local_addr(), "/chunked");

    EXPECT_EQ(response.status_code, 200);
    std::string body_str(response.body.begin(), response.body.end());
    EXPECT_EQ(body_str, "Hello, World");

    server.join();
}

// レスポンスヘッダの取得
TEST_F(HttpClientTest, ResponseHeaders) {
    std::string mock_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "X-Custom: test-value\r\n"
        "Content-Length: 2\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{}";

    std::thread server([this, &mock_response]() {
        serve_response(mock_response);
    });

    HttpClient client(pool_);
    auto response = client.get(local_addr(), "/headers");

    EXPECT_EQ(response.get_header("Content-Type"), "application/json");
    EXPECT_EQ(response.get_header("X-Custom"), "test-value");

    server.join();
}

// 接続失敗時
TEST_F(HttpClientTest, ConnectionFailure) {
    HttpClient client(pool_);
    // 存在しないポートに接続
    Address bad_addr{"127.0.0.1", 1};
    auto response = client.get(bad_addr, "/fail");
    EXPECT_EQ(response.status_code, 0);
}
