#pragma once

/// ergo_package_model — ビルドディレクトリから PackageModel を組み立てる入口。
///
/// 順に: resolved.json (必須) → File API codemodel (`require_codemodel` なら必須、
/// それ以外は無ければ飛ばす) → 帰属と既定の除外 → `.anatomia/layers.json` の層。
/// 層宣言のパスを省くと `<ergo_root>/.anatomia/layers.json` を探す (無ければ層なし)。

#include "ergo/package_model/file_api_reader.h"
#include "ergo/package_model/package_model.h"

#include <string>
#include <vector>

namespace ergo::package_model {

struct LoadOptions {
    std::string build_dir;
    std::string resolved_path;   ///< 空なら <build_dir>/ergo-packages.resolved.json
    std::string layers_path;     ///< 空なら <ergo_root>/.anatomia/layers.json
    bool        require_codemodel = false;
    CodeModelOptions codemodel;
};

struct LoadReport {
    std::vector<std::string> notes;   ///< 飛ばした入力など (図の題・CLI の表示に使う)
};

bool load_package_model(const LoadOptions& options, PackageModel& model,
                        LoadReport* report = nullptr, std::string* error = nullptr);

} // namespace ergo::package_model
