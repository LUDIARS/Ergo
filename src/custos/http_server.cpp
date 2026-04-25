#include "http_server.h"

#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   using socket_t = SOCKET;
   static constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
   static int  close_socket(socket_t s) { return ::closesocket(s); }
   static int  last_socket_error()      { return WSAGetLastError(); }
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  include <errno.h>
   using socket_t = int;
   static constexpr socket_t INVALID_SOCK = -1;
   static int  close_socket(socket_t s) { return ::close(s); }
   static int  last_socket_error()      { return errno; }
#endif

namespace ergo::custos::detail {

namespace {

#if defined(_WIN32)
struct WsaScope {
    bool ok = false;
    WsaScope() {
        WSADATA d{};
        ok = (WSAStartup(MAKEWORD(2, 2), &d) == 0);
    }
    ~WsaScope() { if (ok) WSACleanup(); }
};
static WsaScope g_wsa;     // プロセス寿命
#endif

/// 文字列 比較 (大文字小文字無視)。
bool iequals(const std::string& a, const char* b) {
    if (a.size() != std::strlen(b)) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        char x = a[i] >= 'a' && a[i] <= 'z' ? char(a[i] - 32) : a[i];
        char y = b[i] >= 'a' && b[i] <= 'z' ? char(b[i] - 32) : b[i];
        if (x != y) return false;
    }
    return true;
}

/// HTTP/1.0 リクエストをパースする。Content-Length までを最低限読む。
bool read_request(socket_t fd, HttpRequest& out) {
    constexpr std::size_t MAX_HEADER = 16 * 1024;
    std::string buf;
    buf.reserve(2048);

    // ヘッダ終端 (\r\n\r\n) を見つけるまで読む。
    while (buf.size() < MAX_HEADER) {
        char tmp[1024];
        int n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0) return false;
        buf.append(tmp, std::size_t(n));
        if (buf.find("\r\n\r\n") != std::string::npos) break;
    }
    auto header_end = buf.find("\r\n\r\n");
    if (header_end == std::string::npos) return false;

    // Request line
    auto first_eol = buf.find("\r\n");
    if (first_eol == std::string::npos) return false;
    {
        std::string line = buf.substr(0, first_eol);
        auto sp1 = line.find(' ');
        if (sp1 == std::string::npos) return false;
        auto sp2 = line.find(' ', sp1 + 1);
        if (sp2 == std::string::npos) return false;
        out.method = line.substr(0, sp1);
        out.path   = line.substr(sp1 + 1, sp2 - sp1 - 1);
    }

    // Content-Length
    std::size_t content_length = 0;
    {
        std::size_t pos = first_eol + 2;
        while (pos < header_end) {
            auto eol = buf.find("\r\n", pos);
            if (eol == std::string::npos || eol > header_end) break;
            std::string line = buf.substr(pos, eol - pos);
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string name = line.substr(0, colon);
                std::string val  = line.substr(colon + 1);
                while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(0, 1);
                if (iequals(name, "Content-Length")) {
                    content_length = std::stoul(val);
                }
            }
            pos = eol + 2;
        }
    }

    // Body をヘッダ後ろに残っている分 + 必要に応じて追加 recv
    std::size_t body_start = header_end + 4;
    std::string body = buf.substr(body_start);
    while (body.size() < content_length) {
        char tmp[2048];
        int  n = ::recv(fd, tmp, int(sizeof(tmp)), 0);
        if (n <= 0) return false;
        body.append(tmp, std::size_t(n));
    }
    if (body.size() > content_length) body.resize(content_length);
    out.body = std::move(body);
    return true;
}

void write_all(socket_t fd, const char* data, std::size_t len) {
    std::size_t sent = 0;
    while (sent < len) {
        int n = ::send(fd, data + sent, int(len - sent), 0);
        if (n <= 0) return;
        sent += std::size_t(n);
    }
}

const char* status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 503: return "Service Unavailable";
        default:  return "Status";
    }
}

void write_response(socket_t fd, const HttpResponse& res) {
    std::string head;
    head.reserve(256);
    head += "HTTP/1.0 ";
    head += std::to_string(res.status);
    head += ' ';
    head += status_text(res.status);
    head += "\r\nConnection: close\r\nContent-Type: ";
    head += res.content_type;
    head += "\r\nContent-Length: ";
    head += std::to_string(res.body.size());
    head += "\r\nCache-Control: no-store\r\n";
    for (const auto& kv : res.extra_headers) {
        head += kv.first;
        head += ": ";
        head += kv.second;
        head += "\r\n";
    }
    head += "\r\n";
    write_all(fd, head.data(), head.size());
    if (!res.body.empty()) {
        write_all(fd, reinterpret_cast<const char*>(res.body.data()), res.body.size());
    }
}

} // anonymous

// ─── HttpResponse helpers ───────────────────────

HttpResponse HttpResponse::text(int status, const std::string& msg) {
    HttpResponse r;
    r.status = status;
    r.content_type = "text/plain; charset=utf-8";
    r.body.assign(msg.begin(), msg.end());
    return r;
}
HttpResponse HttpResponse::png(std::vector<std::uint8_t>&& bytes) {
    HttpResponse r;
    r.status = 200;
    r.content_type = "image/png";
    r.body = std::move(bytes);
    return r;
}
HttpResponse HttpResponse::json(int status, const std::string& body) {
    HttpResponse r;
    r.status = status;
    r.content_type = "application/json; charset=utf-8";
    r.body.assign(body.begin(), body.end());
    return r;
}

// ─── HttpServer ────────────────────────────────

HttpServer::~HttpServer() { shutdown(); }

bool HttpServer::start(const std::string& host, std::uint16_t port, RequestHandler handler) {
    if (running_) return true;
    handler_ = std::move(handler);

#if defined(_WIN32)
    if (!g_wsa.ok) return false;
#endif

    listen_fd_ = int(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (listen_fd_ == INVALID_SOCK) return false;

    int yes = 1;
    ::setsockopt(socket_t(listen_fd_), SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (host == "0.0.0.0" || host.empty()) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
#if defined(_WIN32)
        if (InetPtonA(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
            close_socket(socket_t(listen_fd_));
            listen_fd_ = -1;
            return false;
        }
#else
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
            close_socket(socket_t(listen_fd_));
            listen_fd_ = -1;
            return false;
        }
#endif
    }

    if (::bind(socket_t(listen_fd_), reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::fprintf(stderr, "[ergo_custos] bind %s:%u failed (errno=%d)\n",
                     host.c_str(), unsigned(port), last_socket_error());
        close_socket(socket_t(listen_fd_));
        listen_fd_ = -1;
        return false;
    }
    if (::listen(socket_t(listen_fd_), 4) != 0) {
        close_socket(socket_t(listen_fd_));
        listen_fd_ = -1;
        return false;
    }

    // bind 後の実際のポートを記録 (port=0 で OS 任せの場合に有用)
    sockaddr_in actual{};
    socklen_t   alen = sizeof(actual);
    if (::getsockname(socket_t(listen_fd_), reinterpret_cast<sockaddr*>(&actual), &alen) == 0) {
        port_ = ntohs(actual.sin_port);
    } else {
        port_ = port;
    }

    running_ = true;
    accept_thread_ = std::thread([this] { accept_loop(); });
    return true;
}

void HttpServer::shutdown() {
    if (!running_) return;
    running_ = false;
    if (listen_fd_ != -1) {
        // shutdown() で accept をブロック解除する
#if defined(_WIN32)
        ::shutdown(socket_t(listen_fd_), SD_BOTH);
#else
        ::shutdown(socket_t(listen_fd_), SHUT_RDWR);
#endif
        close_socket(socket_t(listen_fd_));
        listen_fd_ = -1;
    }
    if (accept_thread_.joinable()) accept_thread_.join();
}

void HttpServer::accept_loop() {
    while (running_) {
        sockaddr_in client{};
        socklen_t   clen = sizeof(client);
        socket_t    cs   = ::accept(socket_t(listen_fd_),
                                    reinterpret_cast<sockaddr*>(&client), &clen);
        if (cs == INVALID_SOCK) {
            if (!running_) break;
            continue;
        }
        handle_connection(int(cs));
        close_socket(cs);
    }
}

void HttpServer::handle_connection(int client_fd) {
    HttpRequest req;
    if (!read_request(socket_t(client_fd), req)) {
        auto r = HttpResponse::text(400, "bad request");
        write_response(socket_t(client_fd), r);
        return;
    }
    HttpResponse res;
    try {
        res = handler_ ? handler_(req) : HttpResponse::not_implemented();
    } catch (const std::exception& e) {
        res = HttpResponse::text(500, std::string("handler exception: ") + e.what());
    } catch (...) {
        res = HttpResponse::text(500, "handler exception: unknown");
    }
    write_response(socket_t(client_fd), res);
}

} // namespace ergo::custos::detail
