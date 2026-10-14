#pragma once

// 由来の取得元 (spec/feature/package-manager.md §3「由来」、 段階 W4)。 ライブラリはこの interface だけを持ち、
// HTTP は tools/package_cli のアダプタ (ergo_http) に置く (テストは偽ソースと JSON 見本)。
//
//   IReviewSource  Revisor の local PR (open な全件と merged)。 GET /v1/local-prs
//   IRunSource     Concordia の委託 run。 GET /v1/delegation/runs
//
// どちらも 1 回の watch で 1 度だけ呼ぶ。 届かない・認可されないときは available = false と理由。
// PR のタイトル・本文は DTO に持たない (§8)。 head_ref は run との突合にだけ使い、 報告には載せない。

#include <string>
#include <vector>

namespace ergo::package_watch {

struct LocalPr {
    std::string repository;  ///< Revisor の repository (例 "LUDIARS/Pictor")
    int         number = 0;
    std::string status;      ///< "open" / "merged" / "closed"
    std::string base_ref;
    std::string head_ref;    ///< 突合だけに使う
    std::string head_sha;
    std::string session_id;  ///< 提出したセッション (突合だけに使う)
    std::string merge_sha;   ///< merged: main に入った commit
};

struct ReviewListing {
    bool                 available = false;
    std::string          reason;  ///< available == false の理由 (報告には載せない)
    std::vector<LocalPr> open;
    std::vector<LocalPr> merged;
};

struct DelegationRun {
    std::string id;
    std::string status;            ///< Concordia の run の状態 (queued / running / completed ...)
    std::string triggered_by;
    std::string child_session_id;  ///< 突合だけに使う
    std::string spawn_branch;      ///< 突合だけに使う
    std::string repository;        ///< 対象リポの名前 (repo_key の形。 パスは持たない)
};

struct RunListing {
    bool                       available = false;
    std::string                reason;
    std::vector<DelegationRun> runs;
};

class IReviewSource {
public:
    virtual ~IReviewSource() = default;
    virtual ReviewListing list_reviews() = 0;
};

class IRunSource {
public:
    virtual ~IRunSource() = default;
    virtual RunListing list_runs() = 0;
};

} // namespace ergo::package_watch
