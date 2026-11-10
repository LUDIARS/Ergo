#pragma once
/// Test double for platform::IHttpClient: answers by "METHOD URL" and
/// records every request. Unknown routes fail at the transport level.
#include "ergo/studio/platform/http_client.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace studio_test {

/// Answers by "METHOD URL"; records every request.
class FakeHttp final : public ergo::studio::platform::IHttpClient {
public:
    std::map<std::string, std::pair<int, std::string>> routes;
    std::vector<ergo::studio::platform::HttpClientRequest>           seen;

    ergo::studio::platform::HttpClientResponse send(const ergo::studio::platform::HttpClientRequest& req) override {
        seen.push_back(req);
        ergo::studio::platform::HttpClientResponse r;
        const auto it = routes.find(req.method + " " + req.url);
        if (it == routes.end()) {
            r.error = "no route";
            return r;
        }
        r.transport_ok = true;
        r.status       = it->second.first;
        r.body         = it->second.second;
        return r;
    }
    std::string header(std::size_t i, const std::string& name) const {
        for (const auto& [k, v] : seen.at(i).headers) {
            if (k == name) return v;
        }
        return {};
    }
};

} // namespace studio_test
