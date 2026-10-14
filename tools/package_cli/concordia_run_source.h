#pragma once

// IRunSource の Concordia 版 (ergo_http)。 1 回の list_runs で 1 往復:
//
//   GET <concordia>/v1/delegation/runs?limit=500   最近の run (進行中と、 local PR を出し終えた run の突合用)

#include "ergo/http/client.hpp"
#include "ergo/package_watch/provenance_source.h"

#include <memory>
#include <string>

namespace ergo::package_cli {

class ConcordiaRunSource : public ergo::package_watch::IRunSource {
public:
    ConcordiaRunSource(std::shared_ptr<ergo::http::IHttpClient> client, std::string base_url);
    ergo::package_watch::RunListing list_runs() override;

private:
    std::shared_ptr<ergo::http::IHttpClient> client_;
    std::string                              base_;
};

} // namespace ergo::package_cli
