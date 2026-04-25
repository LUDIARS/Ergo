#pragma once
//
// 軽量 HTTP/1.0 サーバ。ergo_custos 専用、外部ライブラリゼロ。
//
// 1 接続 = 1 リクエスト → レスポンス → close (Connection: close)。
// 並列度は 1 で十分 (Custos backend からの逐次呼び出し前提)。
// アクセプトは別スレッドで blocking ループ。

#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

namespace ergo::custos::detail {

struct HttpRequest {
    std::string method;     // "GET" / "POST"
    std::string path;       // "/screenshot"
    std::string body;       // POST body (空可)
};

struct HttpResponse {
    int                       status        = 200;
    std::string               content_type  = "text/plain; charset=utf-8";
    std::vector<std::uint8_t> body;
    /// `Connection: close` 等を上書きしたいときの追加ヘッダ。
    std::vector<std::pair<std::string, std::string>> extra_headers;

    static HttpResponse text(int status, const std::string& msg);
    static HttpResponse png (std::vector<std::uint8_t>&& bytes);
    static HttpResponse json(int status, const std::string& body);
    static HttpResponse not_found()      { return text(404, "not found");    }
    static HttpResponse not_implemented(){ return text(501, "not implemented"); }
};

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer {
public:
    HttpServer() = default;
    ~HttpServer();
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    /// listen 開始。bind 失敗時 false。
    bool start(const std::string& host, std::uint16_t port, RequestHandler handler);

    /// 別スレッドの accept ループを止めて socket close。
    void shutdown();

    bool         is_running() const noexcept { return running_; }
    std::uint16_t bound_port() const noexcept { return port_; }

private:
    void accept_loop();
    void handle_connection(int client_fd);

    int                 listen_fd_ = -1;
    std::uint16_t       port_      = 0;
    std::atomic<bool>   running_{false};
    std::thread         accept_thread_;
    RequestHandler      handler_;
};

} // namespace ergo::custos::detail
