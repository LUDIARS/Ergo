#pragma once

// 更新の判定 (spec/feature/package-manager.md §3 のうち W3 の tag / track / off-main)。 観測から
// 「今そこにある更新」を作る純粋関数。 前回状態は見ない (重複の抑止は watch_diff.h)。
//
//   tag       pin より新しい v* タグ (pin の子孫で expect_tag より新しい)。 主体 = タグ名
//   track     追跡ブランチの先頭 M が pin と違い、 pin が M の祖先。 主体 = M
//   off-main  pin が追跡ブランチの祖先でない (警告)。 主体 = pin

#include "ergo/package_watch/observation.h"
#include "ergo/package_watch/watch_dependency.h"

#include <optional>
#include <string>
#include <vector>

namespace ergo::package_watch {

enum class UpdateKind { Tag, Track, OffMain };

const char* update_kind_name(UpdateKind kind);
bool        parse_update_kind(const std::string& text, UpdateKind& out);

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
};

/// 並びは kind 順 (tag → track → off-main)。 観測できていなければ空。
std::vector<Candidate> detect_updates(const WatchDependency& dep, const Observation& obs);

} // namespace ergo::package_watch
