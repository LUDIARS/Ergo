#pragma once

// ergo_pkg watch が由来 (W4) を読む先 (spec/feature/package-manager.md §3)。
//
//   Revisor    --revisor-url   > ERGO_WATCH_REVISOR_URL
//   Concordia  --concordia-url > ERGO_WATCH_CONCORDIA_URL
//
// loopback 以外は引数の誤り (終了 2)。 渡されなければその取得元は見ない (報告の sources.* は "off")。
// ポートの数字は Ergo に書かない (Excubitor の catalog が正本で、 ジョブが渡す)。 読み取りに認可は要らない
// (Revisor の GET /v1/local-prs と Concordia の GET /v1/delegation/runs は loopback から読める)。

#include "cli_args.h"

#include <string>

namespace ergo::package_cli {

struct ProvenanceEndpoints {
    std::string revisor;    ///< 末尾の "/" を落とした基底 URL (空なら見ない)
    std::string concordia;
};

/// loopback 以外の URL なら false と理由。
bool resolve_provenance_endpoints(const Args& args, ProvenanceEndpoints& out, std::string* error);

} // namespace ergo::package_cli
