#pragma once

// ergo_pkg watch の由来の取得元を組み立てる。 URL が無い取得元は作らない (nullptr = 見ない)。
// ergo_http 無しで作った CLI (ERGO_BUILD_HTTP=OFF) では、 URL があっても「届かない」取得元を返す
// (報告の sources.* は "unavailable"、 run / review は出ない)。

#include "provenance_endpoints.h"

#include "ergo/package_watch/provenance_source.h"

#include <memory>

namespace ergo::package_cli {

struct ProvenanceSources {
    std::unique_ptr<ergo::package_watch::IReviewSource> reviews;
    std::unique_ptr<ergo::package_watch::IRunSource>    runs;
};

ProvenanceSources make_provenance_sources(const ProvenanceEndpoints& endpoints);

} // namespace ergo::package_cli
