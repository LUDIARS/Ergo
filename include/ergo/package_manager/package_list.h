#pragma once

// `ergo_pkg list` の一覧: ホストの宣言・Ergo のマニフェスト・resolved.json・UPM の宣言を合わせて
// 1 パッケージ 1 行にする。 どれが欠けても作れる (欠けた欄は空)。

#include "ergo/common/json_min.h"
#include "ergo/package_manager/ergo_manifest.h"
#include "ergo/package_manager/host_declaration.h"
#include "ergo/package_manager/upm_declaration.h"

#include <string>
#include <vector>

namespace ergo::package_manager {

namespace jsonm = ergo::common::jsonm;

struct ListRow {
    std::string name;
    std::string owner;       ///< pin の持ち主: "host" (Ergo 自身・非公開) / "ergo" (共有) / "upm"
    std::string kind;        ///< "ergo" / "link" / "tool" / "upm"
    std::string code;
    std::string rev;
    std::string expect_tag;
    bool        used     = false;  ///< host.use にある (Ergo 自身は常に true)
    bool        optional = false;
    std::string features;          ///< host.features の値
    std::string route;             ///< resolved.json の route (無ければ "")
    std::string state;             ///< resolved.json の state (無ければ "")
};

struct ListInput {
    const HostDeclaration* host     = nullptr;
    const ErgoManifest*    ergo     = nullptr;
    const jsonm::JsonValue* resolved = nullptr;  ///< <build>/ergo-packages.resolved.json
    const UpmDeclaration*  upm      = nullptr;
};

/// 並びは Ergo 自身 → 共有 (名前順) → 非公開 (名前順) → UPM (名前順)。
std::vector<ListRow> make_list(const ListInput& input);

/// 人向けの表 (固定幅、 末尾改行)。
std::string render_list_text(const std::vector<ListRow>& rows);

/// 機械向け: { "host": <code>, "packages": [ {...}, ... ] }
jsonm::JsonValue list_to_json(const std::vector<ListRow>& rows, const std::string& host_code);

} // namespace ergo::package_manager
