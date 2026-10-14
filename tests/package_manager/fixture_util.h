#pragma once

// test_package_manager の共通部品: 見本 (fixtures/) と Ergo のファイルを読む。

#include "ergo/common/json_min.h"

#include <fstream>
#include <sstream>
#include <string>

namespace ergo::package_manager::test {

inline std::string read_text(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline std::string fixture_text(const std::string& name) {
    return read_text(std::string(ERGO_PACKAGE_MANAGER_FIXTURES) + "/" + name);
}

inline ergo::common::jsonm::JsonValue fixture_json(const std::string& name) {
    ergo::common::jsonm::JsonValue v;
    ergo::common::jsonm::parse(fixture_text(name), v);
    return v;
}

/// Ergo の cmake/packages/ergo-bootstrap.cmake.in (正本のひな形)。
inline std::string bootstrap_template() {
    return read_text(std::string(ERGO_PACKAGE_MANAGER_SOURCE) + "/cmake/packages/ergo-bootstrap.cmake.in");
}

} // namespace ergo::package_manager::test
