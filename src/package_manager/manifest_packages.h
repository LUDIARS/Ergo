#pragma once

// ergo_package_manager の内部部品: `packages` 節 (Ergo のマニフェストとホスト宣言の非公開 packages で同じ形)
// を ManifestPackage の並び (名前順) にする。

#include "ergo/common/json_min.h"
#include "ergo/package_manager/ergo_manifest.h"
#include "ergo/package_manager/finding.h"

#include <vector>

namespace ergo::package_manager::detail {

/// entry の `track.branch` (追跡ブランチ、 W3)。 無ければ ""、 型違いは findings (MANIFEST)。
std::string read_track_branch(const jsonm::JsonValue& entry, const std::string& where,
                              std::vector<Finding>& findings);

/// doc の `packages` を読む。 無ければ空。 型違いは findings (MANIFEST)。
std::vector<ManifestPackage> read_packages(const jsonm::JsonValue& doc,
                                           std::vector<Finding>& findings);

} // namespace ergo::package_manager::detail
