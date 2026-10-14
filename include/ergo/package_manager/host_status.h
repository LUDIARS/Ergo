#pragma once

// `ergo-packages.status.json` の書き手 (spec/feature/package-manager.md §5.1、 形式 spec/data/ergo-packages-status.md)。
// ergo_pkg status が集めた値を JSON にするだけ (ファイル・git・時計に触らない)。 読み手は ergo_package_model。
// 載せるのはホストのコード・HEAD・構成時刻・テストの数だけで、 パス・テスト名・出力は載せない (§8)。

#include "ergo/common/json_min.h"
#include "ergo/package_manager/test_results.h"

#include <string>

namespace ergo::package_manager {

namespace jsonm = ergo::common::jsonm;

struct HostStatusInput {
    std::string host;            ///< ホストのコード ("" なら "")
    std::string head;            ///< ホストの HEAD (40 桁。 取れなければ "")
    std::string configured_at;   ///< CMakeCache.txt の更新時刻 `YYYY-MM-DDTHH:MM:SSZ` (無ければ "")
    TestTally   tests;           ///< source が unknown なら数を書かない
};

/// schema 1 の JSON。 tests は source が unknown なら `{"source":"unknown"}` だけ。
jsonm::JsonValue host_status_to_json(const HostStatusInput& in);

/// 書くバイト列 (キーは辞書順の 1 行、 改行 1 つで終わる)。 同じ入力なら同じバイト列。
std::string host_status_text(const HostStatusInput& in);

} // namespace ergo::package_manager
