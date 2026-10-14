#pragma once

// ergo_package_manager の内部部品: jsonm の値から型付きで取り出し、 型違いを MANIFEST の error にする。

#include "ergo/common/json_min.h"
#include "ergo/package_manager/finding.h"

#include <map>
#include <string>
#include <vector>

namespace ergo::package_manager::detail {

namespace jsonm = ergo::common::jsonm;

/// 位置付きで MANIFEST の error を積む。
void manifest_error(std::vector<Finding>& findings, const std::string& subject,
                    const std::string& message);

/// 文字列のメンバ。 無い・null は ""、 他の型は error。
std::string string_field(const jsonm::JsonValue& obj, const std::string& key,
                         const std::string& where, std::vector<Finding>& findings);

/// 文字列配列のメンバ。 無いなら空、 型違い (要素を含む) は error。
std::vector<std::string> string_array_field(const jsonm::JsonValue& obj, const std::string& key,
                                            const std::string& where,
                                            std::vector<Finding>& findings);

/// 値が文字列のオブジェクト。 無いなら空、 型違いは error。
std::map<std::string, std::string> string_map_field(const jsonm::JsonValue& obj,
                                                    const std::string& key,
                                                    const std::string& where,
                                                    std::vector<Finding>& findings);

} // namespace ergo::package_manager::detail
