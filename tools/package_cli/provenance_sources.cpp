#include "provenance_sources.h"

#if defined(ERGO_PKG_CLI_WITH_HTTP)
#include "concordia_run_source.h"
#include "revisor_review_source.h"

#include "ergo/http/client.hpp"
#endif

#include <string>
#include <utility>

namespace ergo::package_cli {

namespace pw = ergo::package_watch;

namespace {

#if !defined(ERGO_PKG_CLI_WITH_HTTP)
constexpr const char* kNoHttp = "ergo_pkg was built without ergo_http (configure with -DERGO_BUILD_HTTP=ON)";

class NoHttpReviewSource : public pw::IReviewSource {
public:
    pw::ReviewListing list_reviews() override {
        pw::ReviewListing l;
        l.reason = kNoHttp;
        return l;
    }
};

class NoHttpRunSource : public pw::IRunSource {
public:
    pw::RunListing list_runs() override {
        pw::RunListing l;
        l.reason = kNoHttp;
        return l;
    }
};
#endif

} // namespace

ProvenanceSources make_provenance_sources(const ProvenanceEndpoints& endpoints) {
    ProvenanceSources out;
    if (endpoints.revisor.empty() && endpoints.concordia.empty()) return out;
#if defined(ERGO_PKG_CLI_WITH_HTTP)
    std::shared_ptr<ergo::http::IHttpClient> client = ergo::http::make_curl_client();
    if (!endpoints.revisor.empty()) out.reviews = std::make_unique<RevisorReviewSource>(client, endpoints.revisor);
    if (!endpoints.concordia.empty()) out.runs = std::make_unique<ConcordiaRunSource>(client, endpoints.concordia);
#else
    if (!endpoints.revisor.empty()) out.reviews = std::make_unique<NoHttpReviewSource>();
    if (!endpoints.concordia.empty()) out.runs = std::make_unique<NoHttpRunSource>();
#endif
    return out;
}

} // namespace ergo::package_cli
