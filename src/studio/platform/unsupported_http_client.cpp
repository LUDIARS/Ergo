// Non-Windows builds without the managed libcurl (ERGO_WITH_CURL=OFF): there
// is no HTTP client. Every request fails with this explicit error instead of
// switching to another path.
#include "ergo/studio/platform/http_client.h"

namespace ergo::studio::platform {
namespace {

class UnsupportedHttpClient final : public IHttpClient {
public:
    HttpClientResponse send(const HttpClientRequest&) override {
        HttpClientResponse r;
        r.error = "HTTP is not available in this build (ergo_studio_platform needs libcurl outside Windows: "
                  "configure with -DERGO_WITH_CURL=ON)";
        return r;
    }
};

} // namespace

std::shared_ptr<IHttpClient> make_system_http_client() { return std::make_shared<UnsupportedHttpClient>(); }

} // namespace ergo::studio::platform
