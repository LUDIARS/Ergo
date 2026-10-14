#pragma once

// ホストの cmake/ergo-bootstrap.cmake (Ergo を見つけて add_subdirectory する 1 ファイル) の生成と古さの判定。
// ひな形の正本は Ergo の cmake/packages/ergo-bootstrap.cmake.in で、 2 行目に生成元の Ergo SHA
// ("# ergo-bootstrap-source: <sha>") を入れる。
//
// 古さ: ファイルを、 そのファイルが名乗る生成元 SHA で今のひな形から作り直した結果と比べる。
// 違えば Stale (ひな形が変わった、 または手で直された)。 改行は LF に揃えて比べる (autocrlf 対策)。

#include <string>

namespace ergo::package_manager {

/// ひな形の置き換え位置。
extern const char* const kBootstrapPlaceholder;
/// ホストのリポからの相対パス。
extern const char* const kBootstrapPath;

std::string render_bootstrap(const std::string& tmpl, const std::string& source_sha);

/// "# ergo-bootstrap-source: <sha>" の <sha>。 無ければ ""。
std::string bootstrap_source(const std::string& text);

enum class BootstrapState {
    Missing,  ///< ファイルが無い
    Current,  ///< 今のひな形どおり
    Stale,    ///< 生成元の印はあるが中身が今のひな形と違う
    Foreign   ///< 生成元の印が無い (手書き)
};

struct BootstrapStatus {
    BootstrapState state = BootstrapState::Missing;
    std::string    source;  ///< 名乗っている生成元 SHA
};

const char* bootstrap_state_name(BootstrapState s);

/// text が nullptr ならファイルが無い。
BootstrapStatus check_bootstrap(const std::string* text, const std::string& tmpl);

} // namespace ergo::package_manager
