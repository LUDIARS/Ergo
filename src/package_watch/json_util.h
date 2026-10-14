#pragma once

// ergo_package_watch の内部部品: jsonm の値の組み立てと取り出し (状態・報告で共通)。

#include "ergo/common/json_min.h"

#include <optional>
#include <string>

namespace ergo::package_watch::detail {

namespace jsonm = ergo::common::jsonm;

inline jsonm::JsonValue str(const std::string& s) { return jsonm::JsonValue::make_string(s); }

inline jsonm::JsonValue boolean(bool b) { return jsonm::JsonValue::make_bool(b); }

inline jsonm::JsonValue opt_int(const std::optional<int>& v) {
    return v ? jsonm::JsonValue::make_number(*v) : jsonm::JsonValue::make_null();
}

/// 文字列のメンバ (無い・型違いは "")。
inline std::string get_string(const jsonm::JsonValue& obj, const char* key) {
    const auto* v = obj.find(key);
    return v ? v->as_string() : std::string();
}

inline bool get_bool(const jsonm::JsonValue& obj, const char* key) {
    const auto* v = obj.find(key);
    return v && v->as_bool();
}

/// 数のメンバ (無い・null・型違いは nullopt)。
inline std::optional<int> get_opt_int(const jsonm::JsonValue& obj, const char* key) {
    const auto* v = obj.find(key);
    if (!v || !v->is_number()) return std::nullopt;
    return static_cast<int>(v->n);
}

} // namespace ergo::package_watch::detail
