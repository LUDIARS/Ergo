#include "concordia_run_source.h"

#include "http_json_get.h"

#include "ergo/package_watch/provenance_json.h"

#include <utility>

namespace ergo::package_cli {

namespace pw = ergo::package_watch;

ConcordiaRunSource::ConcordiaRunSource(std::shared_ptr<ergo::http::IHttpClient> client, std::string base_url)
    : client_(std::move(client)), base_(std::move(base_url)) {}

pw::RunListing ConcordiaRunSource::list_runs() {
    pw::RunListing out;
    const auto got = http_get_json(*client_, base_ + "/v1/delegation/runs?limit=500");
    if (!got.ok) {
        out.reason = got.reason;
        return out;
    }
    std::string why;
    if (!pw::parse_delegation_runs(got.body, out.runs, &why)) {
        out.runs.clear();
        out.reason = why;
        return out;
    }
    out.available = true;
    return out;
}

} // namespace ergo::package_cli
