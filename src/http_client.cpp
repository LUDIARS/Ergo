#include "ergo/network/http_client.h"
#include <sstream>
#include <algorithm>
#include <cstring>

namespace ergo::network {

namespace {

const char* method_to_string(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET:     return "GET";
        case HttpMethod::POST:    return "POST";
        case HttpMethod::PUT:     return "PUT";
        case HttpMethod::DELETE_: return "DELETE";
        case HttpMethod::PATCH:   return "PATCH";
        case HttpMethod::HEAD:    return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
    }
    return "GET";
}

} // anonymous namespace

HttpClient::HttpClient(ConnectionPool& pool, const NetworkConfig& config)
    : pool_(pool)
    , config_(config) {
}

HttpClient::~HttpClient() = default;

HttpResponse HttpClient::send(const Address& addr, const HttpRequest& request) {
    HttpResponse response;

    SocketHandle handle = pool_.acquire(addr);
    if (handle == kInvalidSocket) {
        response.status_code = 0;
        response.status_text = "Connection failed";
        return response;
    }

    // タイムアウト設定
    struct timeval tv{};
    tv.tv_sec = config_.read_timeout_ms / 1000;
    tv.tv_usec = (config_.read_timeout_ms % 1000) * 1000;
    setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));

    auto data = serialize_request(request);
    int sent = ::send(handle, reinterpret_cast<const char*>(data.data()),
                      static_cast<int>(data.size()), 0);
    if (sent <= 0) {
        pool_.discard(handle);
        response.status_code = 0;
        response.status_text = "Send failed";
        return response;
    }

    response = parse_response(handle);

    // Connection: close の場合はプールに返さない
    std::string conn_header = response.get_header("Connection");
    if (conn_header == "close") {
        pool_.discard(handle);
    } else {
        pool_.release(handle, addr);
    }

    return response;
}

HttpResponse HttpClient::get(const Address& addr, const std::string& path,
                              const std::vector<HttpHeader>& headers) {
    HttpRequest req;
    req.method = HttpMethod::GET;
    req.path = path;
    req.host = addr.host;
    req.headers = headers;
    return send(addr, req);
}

HttpResponse HttpClient::post(const Address& addr, const std::string& path,
                               const std::vector<uint8_t>& body,
                               const std::vector<HttpHeader>& headers) {
    HttpRequest req;
    req.method = HttpMethod::POST;
    req.path = path;
    req.host = addr.host;
    req.headers = headers;
    req.body = body;
    return send(addr, req);
}

HttpResponse HttpClient::put(const Address& addr, const std::string& path,
                              const std::vector<uint8_t>& body,
                              const std::vector<HttpHeader>& headers) {
    HttpRequest req;
    req.method = HttpMethod::PUT;
    req.path = path;
    req.host = addr.host;
    req.headers = headers;
    req.body = body;
    return send(addr, req);
}

HttpResponse HttpClient::del(const Address& addr, const std::string& path,
                              const std::vector<HttpHeader>& headers) {
    HttpRequest req;
    req.method = HttpMethod::DELETE_;
    req.path = path;
    req.host = addr.host;
    req.headers = headers;
    return send(addr, req);
}

std::vector<uint8_t> HttpClient::serialize_request(const HttpRequest& request) const {
    std::ostringstream ss;

    // リクエストライン
    ss << method_to_string(request.method) << " " << request.path << " HTTP/1.1\r\n";

    // Host ヘッダ
    ss << "Host: " << request.host << "\r\n";

    // ユーザーヘッダ
    for (const auto& h : request.headers) {
        ss << h.name << ": " << h.value << "\r\n";
    }

    // Content-Length（ボディがある場合）
    if (!request.body.empty()) {
        bool has_content_length = false;
        for (const auto& h : request.headers) {
            if (h.name == "Content-Length") {
                has_content_length = true;
                break;
            }
        }
        if (!has_content_length) {
            ss << "Content-Length: " << request.body.size() << "\r\n";
        }
    }

    // Connection: keep-alive
    bool has_connection = false;
    for (const auto& h : request.headers) {
        if (h.name == "Connection") {
            has_connection = true;
            break;
        }
    }
    if (!has_connection) {
        ss << "Connection: keep-alive\r\n";
    }

    ss << "\r\n";

    std::string header_str = ss.str();
    std::vector<uint8_t> result(header_str.begin(), header_str.end());

    if (!request.body.empty()) {
        result.insert(result.end(), request.body.begin(), request.body.end());
    }

    return result;
}

HttpResponse HttpClient::parse_response(SocketHandle socket) const {
    HttpResponse response;

    // ステータスライン読み取り
    std::string status_line = read_line(socket);
    if (status_line.empty()) {
        response.status_code = 0;
        response.status_text = "Empty response";
        return response;
    }

    // "HTTP/1.1 200 OK" をパース
    size_t first_space = status_line.find(' ');
    if (first_space == std::string::npos) {
        response.status_code = 0;
        response.status_text = "Invalid status line";
        return response;
    }

    std::string version_str = status_line.substr(0, first_space);
    if (version_str == "HTTP/1.0") response.version = HttpVersion::HTTP_1_0;
    else response.version = HttpVersion::HTTP_1_1;

    size_t second_space = status_line.find(' ', first_space + 1);
    std::string code_str = status_line.substr(first_space + 1,
                                               second_space == std::string::npos
                                                   ? std::string::npos
                                                   : second_space - first_space - 1);
    response.status_code = std::stoi(code_str);
    if (second_space != std::string::npos) {
        response.status_text = status_line.substr(second_space + 1);
    }

    // ヘッダ読み取り
    while (true) {
        std::string line = read_line(socket);
        if (line.empty()) break;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        HttpHeader header;
        header.name = line.substr(0, colon);
        // ':' の後のスペースをスキップ
        size_t value_start = colon + 1;
        while (value_start < line.size() && line[value_start] == ' ') {
            ++value_start;
        }
        header.value = line.substr(value_start);
        response.headers.push_back(std::move(header));
    }

    // ボディ読み取り
    std::string transfer_encoding = response.get_header("Transfer-Encoding");
    std::string content_length_str = response.get_header("Content-Length");

    if (transfer_encoding == "chunked") {
        response.body = read_chunked_body(socket);
    } else if (!content_length_str.empty()) {
        size_t content_length = std::stoull(content_length_str);
        response.body = read_fixed_body(socket, content_length);
    }

    return response;
}

std::vector<uint8_t> HttpClient::read_chunked_body(SocketHandle socket) const {
    std::vector<uint8_t> body;

    while (true) {
        std::string size_line = read_line(socket);
        if (size_line.empty()) break;

        size_t chunk_size = std::stoull(size_line, nullptr, 16);
        if (chunk_size == 0) {
            read_line(socket); // trailing CRLF
            break;
        }

        auto chunk = read_fixed_body(socket, chunk_size);
        body.insert(body.end(), chunk.begin(), chunk.end());
        read_line(socket); // chunk 末尾の CRLF
    }

    return body;
}

std::vector<uint8_t> HttpClient::read_fixed_body(SocketHandle socket, size_t length) const {
    std::vector<uint8_t> body(length);
    size_t total_read = 0;

    while (total_read < length) {
        int received = ::recv(socket, reinterpret_cast<char*>(body.data() + total_read),
                              static_cast<int>(length - total_read), 0);
        if (received <= 0) break;
        total_read += static_cast<size_t>(received);
    }

    body.resize(total_read);
    return body;
}

std::string HttpClient::read_line(SocketHandle socket) const {
    std::string line;
    char ch;

    while (true) {
        int received = ::recv(socket, &ch, 1, 0);
        if (received <= 0) break;
        if (ch == '\r') {
            // '\n' を読み飛ばす
            ::recv(socket, &ch, 1, MSG_PEEK);
            if (ch == '\n') {
                ::recv(socket, &ch, 1, 0);
            }
            break;
        }
        if (ch == '\n') break;
        line += ch;
    }

    return line;
}

} // namespace ergo::network
