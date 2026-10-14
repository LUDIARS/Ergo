#pragma once

/// ergo_package_model — CMake File API codemodel v2 の返答を読む。
///
/// `<build>/.cmake/api/v1/reply/` の最新の index (名前順で最後) から、 ハブが置いた
/// クライアント `client-ergo` の codemodel 応答を探す (無ければ index の objects の
/// codemodel)。 `version.major` が 2 でなければ明示エラー (形式変更で黙って誤読しない)。
/// ターゲットごとに名前・型・ソース・直接の依存・定義したディレクトリ・属する
/// project を読む。 生成器が足すターゲット (ALL_BUILD / ZERO_CHECK など) と
/// UTILITY は読まない。 パッケージへの帰属は `attribution.h` が行う。

#include "ergo/package_model/package_model.h"

#include <string>

namespace ergo::package_model {

struct CodeModelOptions {
    /// 複数構成の生成器で使う構成名。 空なら最初の構成。
    std::string configuration;
};

/// codemodel を読んで `model` の File API 部分 (targets ほか) を埋める。
bool read_codemodel(const std::string& build_dir, PackageModel& model,
                    std::string* error = nullptr, const CodeModelOptions& options = {});

/// パスを '/' 区切りの正規形にする (`base` が空でなく `path` が相対なら連結する)。
std::string normalize_path(const std::string& path, const std::string& base = {});

/// `path` が `root` 自身かその下にあるか (Windows のドライブ名は大小を区別しない)。
bool path_within(const std::string& path, const std::string& root);

} // namespace ergo::package_model
