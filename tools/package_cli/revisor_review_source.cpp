#include "revisor_review_source.h"

#include "http_json_get.h"

#include "ergo/package_watch/provenance_json.h"

#include <utility>

namespace ergo::package_cli {

namespace pw = ergo::package_watch;

namespace {

/// 1 つの一覧を読む。 失敗なら false と理由。
bool fetch(ergo::http::IHttpClient& client, const std::string& url, std::vector<pw::LocalPr>& out,
           std::string& reason) {
    const auto got = http_get_json(client, url);
    if (!got.ok) {
        reason = got.reason;
        return false;
    }
    std::string why;
    if (!pw::parse_local_prs(got.body, out, &why)) {
        reason = why;
        return false;
    }
    return true;
}

} // namespace

RevisorReviewSource::RevisorReviewSource(std::shared_ptr<ergo::http::IHttpClient> client, std::string base_url)
    : client_(std::move(client)), base_(std::move(base_url)) {}

pw::ReviewListing RevisorReviewSource::list_reviews() {
    pw::ReviewListing out;
    if (!fetch(*client_, base_ + "/v1/local-prs?view=full&state=open", out.open, out.reason) ||
        !fetch(*client_, base_ + "/v1/local-prs?view=summary&state=merged", out.merged, out.reason)) {
        out.open.clear();
        out.merged.clear();
        return out;
    }
    out.available = true;
    return out;
}

} // namespace ergo::package_cli
