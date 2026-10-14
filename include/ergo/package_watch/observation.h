#pragma once

// 依存 1 つの観測 (spec/feature/package-manager.md §2.3)。 取得元 (IRefSource / IHistorySource) を問い合わせて、
// 判定 (detect.h) に要る事実だけを集める。
//
//   * 指紋が前回と同じならミラーを同期しない (fetch を省く)。 兄弟チェックアウトは同期しない
//   * 取れなければ observed = false と理由 (「更新なし」とは扱わない)
//   * タグは semver の新しい順に見て、 pin より新しい最新の 1 つだけを持つ

#include "ergo/package_watch/ref_source.h"
#include "ergo/package_watch/watch_dependency.h"

#include <optional>
#include <string>

namespace ergo::package_watch {

extern const char* const kAsOfRemote;   ///< "ls-remote"
extern const char* const kAsOfSibling;  ///< "sibling-last-fetch"

struct TagObservation {
    std::string        name;
    std::string        rev;
    std::optional<int> distance;  ///< pin..タグ のコミット数
};

struct Observation {
    bool                          observed = false;
    std::string                   reason;
    std::string                   as_of;           ///< kAsOfRemote / kAsOfSibling
    std::string                   fingerprint;
    std::string                   track_head;      ///< 追跡ブランチの先頭 M
    std::optional<bool>           pin_on_track;    ///< pin が M の祖先 (または M そのもの)
    std::optional<int>            behind;          ///< pin..M のコミット数
    std::optional<TagObservation> latest_tag;      ///< pin より新しい最新の v* タグ
    std::optional<int>            sibling_behind;  ///< 兄弟チェックアウトの HEAD..M
};

/// タグの選び方: 正式版 (pre-release でない) の v* タグを semver の新しい順に、 expect_tag (読めれば) より新しいものだけを見る。 履歴が答えれば
/// pin の子孫で pin と違う最初のもの、 答えなければ expect_tag より新しい最初のもの (expect_tag が無ければ選ばない)。
/// pin そのものを指すタグに着いたらそれより古いものは見ない。
Observation observe_dependency(const WatchDependency& dep, IRefSource& refs,
                               IHistorySource& history, const std::string& previous_fingerprint);

} // namespace ergo::package_watch
