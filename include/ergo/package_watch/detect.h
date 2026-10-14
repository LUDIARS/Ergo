#pragma once

// 更新の判定 (spec/feature/package-manager.md §3 のうち W3 の tag / track / off-main)。 観測から
// 「今そこにある更新」を作る純粋関数。 前回状態は見ない (重複の抑止は watch_diff.h)。
//
//   tag       pin より新しい v* タグ (pin の子孫で expect_tag より新しい)。 主体 = タグ名
//   track     追跡ブランチの先頭 M が pin と違い、 pin が M の祖先。 主体 = M
//   off-main  pin が追跡ブランチの祖先でない (警告)。 主体 = pin
//
// W4 の run / review / merged (由来つき) は provenance.h が作り、 同じ Candidate に載せる。

#include "ergo/package_watch/observation.h"
#include "ergo/package_watch/watch_dependency.h"

#include <optional>
#include <string>
#include <vector>

namespace ergo::package_watch {

/// 並びの順 (状態・出来事の並べ替えに使う)。 Run / Review / Merged は W4 (provenance.h)。
enum class UpdateKind { Tag, Track, OffMain, Run, Review, Merged };

const char* update_kind_name(UpdateKind kind);
bool        parse_update_kind(const std::string& text, UpdateKind& out);

/// run / review / merged (由来を持つ種類)。
bool is_provenance_kind(UpdateKind kind);

struct Candidate {
    std::string        dependency;
    UpdateKind         kind = UpdateKind::Tag;
    std::string        subject;    ///< 重複判定の鍵 (タグ名 / M の SHA / pin の SHA)
    std::string        from_rev;   ///< 今の pin
    std::string        from_tag;   ///< 今の expect_tag
    std::string        to_rev;     ///< 上げ先 (off-main は追跡ブランチの先頭)
    std::string        to_tag;     ///< 上げ先のタグ (tag だけ)
    std::optional<int> behind;     ///< pin..上げ先 のコミット数
    bool               warning   = false;  ///< off-main
    bool               strict_ok = false;  ///< 上げ先がタグで、 STRICT で通る
    std::string        next;       ///< 次の操作 (コマンドの形。 実行はしない)

    // run / review / merged だけ (provenance.h)。 PR のタイトル・本文・ブランチ名は持たない (§8)。
    std::string        origin;     ///< "pf" / "ai" / "human" / "unknown"
    std::string        trigger;    ///< ai: run の triggered_by の写し
    std::optional<int> review;     ///< review / merged: local PR の番号
    std::string        head;       ///< review / merged: local PR の head の SHA7
};

/// 並びは kind 順 (tag → track → off-main)。 観測できていなければ空。
std::vector<Candidate> detect_updates(const WatchDependency& dep, const Observation& obs);

} // namespace ergo::package_watch
