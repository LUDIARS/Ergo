#pragma once

// 由来の判定 (spec/feature/package-manager.md §3、 段階 W4)。 Revisor の local PR と Concordia の委託 run から
// 依存 1 つの run / review / merged を作る純粋関数。
//
//   run     依存のリポを対象にした委託 run が進行中。 主体 = run id
//   review  依存のリポで base が追跡ブランチの open な local PR。 主体 = "#<番号>@<head SHA7>"
//   merged  前回 review だった local PR が merged になった。 主体 = "#<番号>"。 1 回だけ出し、 状態には残さない
//
// 由来 (origin):
//   pf       PR の sessionId が run の child_session_id と一致 (無ければ headRef == spawn_branch かつ同じリポ) し、
//            run の triggered_by が kPraeformaTrigger
//   ai       同じ突合で run が見つかり、 triggered_by がそれ以外 (trigger に写す)
//   human    Concordia に届き、 突合する run が無い
//   unknown  Concordia に届かない・認可されない・見ていない
//
// 出来事には PR のタイトル・本文・ブランチ名・セッション id を載せない (§8)。 local PR は番号と head SHA7 だけ。

#include "ergo/package_watch/detect.h"
#include "ergo/package_watch/provenance_source.h"
#include "ergo/package_watch/watch_dependency.h"
#include "ergo/package_watch/watch_diff.h"
#include "ergo/package_watch/watch_state.h"

#include <string>
#include <vector>

namespace ergo::package_watch {

/// Praeforma の委託の triggered_by (Pf の cc-client が送る定数)。
extern const char* const kPraeformaTrigger;

extern const char* const kOriginPf;
extern const char* const kOriginAi;
extern const char* const kOriginHuman;
extern const char* const kOriginUnknown;

/// 報告の sources.* の値。
extern const char* const kSourceOk;           ///< "ok"
extern const char* const kSourceUnavailable;  ///< "unavailable" (届かない・認可されない・形が違う)
extern const char* const kSourceOff;          ///< "off" (URL が渡されていない)

/// "LUDIARS/Pictor" / "https://host/LUDIARS/Pictor.git" / "<dir>/Pictor" → "pictor" (末尾名を小文字で)。
std::string repo_key(const std::string& text);

/// run が進行中 (queued / launching / pending / spawned / running / blocked)。
bool is_run_active(const std::string& status);

struct OriginResult {
    std::string origin;   ///< kOrigin*
    std::string trigger;  ///< ai のときの triggered_by
};

/// runs が nullptr か available == false なら unknown。
OriginResult classify_review(const LocalPr& pr, const RunListing* runs);

/// 由来の取得元の今回の結果。 nullptr はその取得元を見ていない (URL 無し)。
struct ProvenanceInputs {
    const ReviewListing* reviews = nullptr;
    const RunListing*    runs    = nullptr;
};

struct WatchSources {
    std::string revisor   = kSourceOff;
    std::string concordia = kSourceOff;
};

WatchSources source_states(const ProvenanceInputs& in);

struct ProvenanceResult {
    bool                   reviews_observed = false;  ///< review / merged を判定した (Revisor に届いた)
    bool                   runs_observed    = false;  ///< run を判定した (Concordia に届いた)
    std::vector<Candidate> updates;                   ///< run → review → merged の順
};

/// 前回状態にある、 この依存の review (merged の判定に使う)。
std::vector<Candidate> previous_reviews(const WatchState& previous, const std::string& dependency);

/// 依存 1 つの run / review / merged。 merged は previous_reviews にある番号だけ (由来はそこから写す)。
ProvenanceResult detect_provenance(const WatchDependency& dep, const ProvenanceInputs& in,
                                   const std::vector<Candidate>& previous_reviews);

/// ホストの全ての結果に由来を足す (updates に追記し、 *_observed を立てる)。
void attach_provenance(std::vector<DependencyResult>& results, const ProvenanceInputs& in,
                       const WatchState& previous);

} // namespace ergo::package_watch
