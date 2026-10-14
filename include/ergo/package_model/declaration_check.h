#pragma once

/// ergo_package_model — PKG-17 宣言漏れ: File API の実リンクのうち、 マニフェストの宣言
/// (`modules.<m>.needs` / `optional`、 `packages.<p>.requires`) に無いパッケージ間リンク。
///
/// 判定するのは「Ergo または link パッケージのターゲット → 別の link パッケージの
/// ターゲット」の直接の依存だけ (host・第三者・tool は宣言の対象外)。
///   * Ergo 側: ターゲット名に最長一致するモジュール (`<module>` か `<module>_…`) の
///     needs / optional に相手のパッケージがあれば宣言済み。 当たるモジュールが無ければ漏れ。
///   * link 側: 自分の `requires` に相手があれば宣言済み。
/// 既定で除外したターゲット (テストなど) は見ない。 構成時には判定しない
/// (File API の返答は generate の後にしかできない)。

#include "ergo/package_model/package_model.h"

#include <string>
#include <vector>

namespace ergo::package_model {

struct UndeclaredLink {
    std::string from_package;
    std::string from_target;
    std::string to_package;
    std::string to_target;
    std::string module;   ///< Ergo 側で当たったモジュール ("" = 当たらない)
};

/// ターゲット名に最長一致するモジュール名 ("" = 無し)。
std::string module_for_target(const PackageModel& model, const std::string& target_name);

/// 宣言漏れを (from_target, to_target) の順で返す。
std::vector<UndeclaredLink> find_undeclared_links(const PackageModel& model,
                                                  bool include_excluded = false);

/// 1 行の説明 (CLI の --check と札の注記に使う)。
std::string describe(const UndeclaredLink& link);

} // namespace ergo::package_model
