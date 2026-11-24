#include "ergo/studio/registry/fetch.h"

#include <portus/net.hpp>

namespace ergo::studio::registry {
namespace {

// 1 往復だけを行う。3xx は return_redirects で頭 (Location) だけを受け取り、
// 転送先に従うかは呼び出し側 (許可したホストの確認) が決める。
class PortusTransport final : public IAssetTransport {
public:
    AssetResponse get(const std::string& url, int timeout_ms, std::size_t limit) override {
        try {
            portus::FetchRequest request;
            request.timeout_ms = timeout_ms;
            request.max_response_bytes = limit;
            request.return_redirects = true;
            auto reply = portus::fetch(url, request);
            const bool redirect = reply.status >= 300 && reply.status < 400 && reply.status != 304;
            if (redirect && reply.headers.find("location") == nullptr) return {};
            return {reply.status, reply.headers.get("location"), std::move(reply.body)};
        } catch (const portus::FetchError&) {
            return {};
        } catch (const std::invalid_argument&) {
            return {};
        }
    }
};

} // namespace

std::unique_ptr<IAssetTransport> make_portus_transport() {
    return std::make_unique<PortusTransport>();
}

} // namespace ergo::studio::registry
