#pragma once

// IReviewSource の Revisor 版 (ergo_http)。 1 回の list_reviews で 2 往復:
//
//   GET <revisor>/v1/local-prs?view=full&state=open      突合に要る headRef / sessionId / baseRef を持つ
//   GET <revisor>/v1/local-prs?view=summary&state=merged  merged の番号と mergeCommitSha
//
// 応答のタイトル・本文は provenance_json の読み手が捨てる。

#include "ergo/http/client.hpp"
#include "ergo/package_watch/provenance_source.h"

#include <memory>
#include <string>

namespace ergo::package_cli {

class RevisorReviewSource : public ergo::package_watch::IReviewSource {
public:
    RevisorReviewSource(std::shared_ptr<ergo::http::IHttpClient> client, std::string base_url);
    ergo::package_watch::ReviewListing list_reviews() override;

private:
    std::shared_ptr<ergo::http::IHttpClient> client_;
    std::string                              base_;
};

} // namespace ergo::package_cli
