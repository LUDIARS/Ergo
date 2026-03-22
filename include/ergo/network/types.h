#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>

// Platform socket type abstraction
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using SocketHandle = SOCKET;
  constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
  constexpr int kSocketError = SOCKET_ERROR;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  using SocketHandle = int;
  constexpr SocketHandle kInvalidSocket = -1;
  constexpr int kSocketError = -1;
#endif

namespace ergo::network {

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

enum class SocketProtocol : uint8_t {
    TCP,
    UDP
};

enum class SocketState : uint8_t {
    Closed,
    Connecting,
    Connected,
    Listening,
    Error
};

enum class HttpMethod : uint8_t {
    GET,
    POST,
    PUT,
    DELETE_,
    PATCH,
    HEAD,
    OPTIONS
};

enum class HttpVersion : uint8_t {
    HTTP_1_0,
    HTTP_1_1
};

enum class StreamChannelState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

enum class StreamEventType : uint8_t {
    Connected,
    Disconnected,
    DataReceived,
    Error
};

enum class ConnectionPoolStrategy : uint8_t {
    FIFO,       // 最古の接続を回収
    LRU         // 最も使用されていない接続を回収
};

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

struct Address {
    std::string host;
    uint16_t    port = 0;

    bool operator==(const Address& other) const {
        return host == other.host && port == other.port;
    }
};

struct AddressHash {
    size_t operator()(const Address& addr) const {
        size_t h1 = std::hash<std::string>{}(addr.host);
        size_t h2 = std::hash<uint16_t>{}(addr.port);
        return h1 ^ (h2 << 16);
    }
};

struct ResolvedAddress {
    std::string ip;
    uint16_t    port = 0;
    int         family = AF_INET; // AF_INET or AF_INET6
};

struct HttpHeader {
    std::string name;
    std::string value;
};

struct HttpRequest {
    HttpMethod                  method  = HttpMethod::GET;
    std::string                 path    = "/";
    HttpVersion                 version = HttpVersion::HTTP_1_1;
    std::string                 host;
    std::vector<HttpHeader>     headers;
    std::vector<uint8_t>        body;

    void set_header(const std::string& name, const std::string& value);
    std::string get_header(const std::string& name) const;
};

struct HttpResponse {
    int                         status_code = 0;
    std::string                 status_text;
    HttpVersion                 version = HttpVersion::HTTP_1_1;
    std::vector<HttpHeader>     headers;
    std::vector<uint8_t>        body;

    std::string get_header(const std::string& name) const;
    bool        is_success() const { return status_code >= 200 && status_code < 300; }
};

struct StreamFrame {
    std::vector<uint8_t> data;
};

struct StreamEvent {
    StreamEventType     type;
    std::vector<uint8_t> data;      // DataReceived 時のみ有効
    std::string         message;    // Error / Disconnected 時の詳細
};

using StreamCallback = std::function<void(const StreamEvent&)>;

struct KeepAliveConfig {
    bool        enabled            = true;
    uint32_t    timeout_seconds    = 60;
    uint32_t    max_connections    = 16;
    uint32_t    max_per_host       = 4;
    ConnectionPoolStrategy strategy = ConnectionPoolStrategy::LRU;
};

struct NetworkConfig {
    KeepAliveConfig keep_alive;
    uint32_t        recv_buffer_size    = 8192;
    uint32_t        connect_timeout_ms  = 5000;
    uint32_t        read_timeout_ms     = 30000;
};

// 接続プール内部用
struct PooledConnection {
    SocketHandle    socket      = kInvalidSocket;
    Address         address;
    SocketState     state       = SocketState::Closed;
    std::chrono::steady_clock::time_point last_used;
    bool            in_use      = false;
};

} // namespace ergo::network
