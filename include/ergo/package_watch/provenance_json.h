#pragma once

// Revisor / Concordia の一覧応答 (JSON) の読み手。 必要なキーだけを DTO に写し、 タイトル・本文などは捨てる。
//
//   Revisor    { "pullRequests": [ { number, repository, status, baseRef, headRef, headSha, sessionId,
//                                    mergeCommitSha, ... } ] }  (view=full / view=summary のどちらも読める)
//   Concordia  { "runs": [ { id, status, triggered_by, child_session_id, spawn_branch,
//                            args: { target_repo }, sessions: [ { repo_origin } ] } ] }

#include "ergo/package_watch/provenance_source.h"

#include <string>
#include <vector>

namespace ergo::package_watch {

/// 形が違えば false と理由。 番号の無い PR は読み飛ばす。
bool parse_local_prs(const std::string& text, std::vector<LocalPr>& out, std::string* error);

/// 形が違えば false と理由。 run の対象リポは sessions[].repo_origin、 無ければ args.target_repo の
/// 末尾名 (repo_key) だけを持つ。
bool parse_delegation_runs(const std::string& text, std::vector<DelegationRun>& out, std::string* error);

} // namespace ergo::package_watch
