#pragma once

#include "ergo/network/types.h"
#include "ergo/network/connection_pool.h"
#include <memory>

namespace ergo::network {

/// HTTP クライアント
/// 接続プールを使用した HTTP/1.1 リクエストの送受信を行う
class HttpClient {
public:
    explicit HttpClient(ConnectionPool& pool, const NetworkConfig& config = NetworkConfig{});
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    /// HTTP リクエストを送信し、レスポンスを受信する
    HttpResponse send(const Address& addr, const HttpRequest& request);

    /// GET リクエスト
    HttpResponse get(const Address& addr, const std::string& path,
                     const std::vector<HttpHeader>& headers = {});

    /// POST リクエスト
    HttpResponse post(const Address& addr, const std::string& path,
                      const std::vector<uint8_t>& body,
                      const std::vector<HttpHeader>& headers = {});

    /// PUT リクエスト
    HttpResponse put(const Address& addr, const std::string& path,
                     const std::vector<uint8_t>& body,
                     const std::vector<HttpHeader>& headers = {});

    /// DELETE リクエスト
    HttpResponse del(const Address& addr, const std::string& path,
                     const std::vector<HttpHeader>& headers = {});

private:
    /// HTTP リクエストをバイト列にシリアライズ
    std::vector<uint8_t> serialize_request(const HttpRequest& request) const;

    /// 受信データから HTTP レスポンスをパース
    HttpResponse parse_response(SocketHandle socket) const;

    /// チャンクボディを読み取り
    std::vector<uint8_t> read_chunked_body(SocketHandle socket) const;

    /// 固定長ボディを読み取り
    std::vector<uint8_t> read_fixed_body(SocketHandle socket, size_t length) const;

    /// ソケットから1行読み取り（\r\n 区切り）
    std::string read_line(SocketHandle socket) const;

    ConnectionPool& pool_;
    NetworkConfig   config_;
};

} // namespace ergo::network
