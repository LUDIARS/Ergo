#include "ergo/network/network_system.h"

#ifdef _WIN32
  #pragma comment(lib, "ws2_32.lib")
#endif

namespace ergo::network {

NetworkSystem::NetworkSystem() = default;

NetworkSystem::~NetworkSystem() {
    shutdown();
}

bool NetworkSystem::initialize(const NetworkConfig& config) {
    if (initialized_) return true;

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return false;
    }
#endif

    config_ = config;
    pool_ = std::make_unique<ConnectionPool>(config.keep_alive);
    http_client_ = std::make_unique<HttpClient>(*pool_, config);
    initialized_ = true;
    return true;
}

void NetworkSystem::shutdown() {
    if (!initialized_) return;

    // 全ストリームを切断
    for (auto& stream : streams_) {
        stream->disconnect();
    }
    streams_.clear();

    http_client_.reset();
    pool_.reset();

#ifdef _WIN32
    WSACleanup();
#endif

    initialized_ = false;
}

std::shared_ptr<StreamChannel> NetworkSystem::create_stream() {
    auto stream = std::make_shared<StreamChannel>();
    streams_.push_back(stream);
    return stream;
}

size_t NetworkSystem::cleanup_pools() {
    if (!pool_) return 0;
    return pool_->cleanup();
}

} // namespace ergo::network
