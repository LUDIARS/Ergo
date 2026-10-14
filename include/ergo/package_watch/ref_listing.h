#pragma once

// ref の一覧の解析と指紋。 git の出力 ("<sha><TAB か空白><ref>" の行。 ls-remote と show-ref --dereference が
// 同じ形) を RefListing にする純粋関数で、 プロセスは起こさない。

#include "ergo/package_watch/ref_source.h"

#include <string>

namespace ergo::package_watch {

enum class RefLayout {
    Remote,   ///< refs/heads/<b> がブランチ (ls-remote)
    Sibling,  ///< refs/remotes/origin/<b> がブランチ (兄弟チェックアウト。 origin/HEAD は飛ばす)
};

/// 行を out.heads / out.tags に足す。 refs/tags/<t>^{} は <t> の値を剥がした commit で上書きする。
/// 40 桁 SHA で始まらない行は無視する。
void parse_ref_lines(const std::string& text, RefLayout layout, RefListing& out);

/// heads と tags から作る 16 桁の 16 進 (FNV-1a 64)。 同じ一覧なら同じ値。
std::string listing_fingerprint(const RefListing& listing);

} // namespace ergo::package_watch
