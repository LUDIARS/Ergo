#pragma once

/// ergo_package_model — `.anatomia/layers.json` の層宣言を読み、 Ergo のターゲットに
/// 層を割り当てる (P2 のグループ)。
///
/// glob は Ergo のリポ相対のファイルパスへ完全一致で当てる (`*` = 1 階層、
/// `**` = 任意階層)。 Anatomia のローダーと同じく glob 文字列で並べてから先勝ち。
/// ターゲットの層はソースごとの層の多数決 (同数は層の名前順で先のもの)。
/// Ergo に帰属しないターゲットと、 どのソースも当たらないターゲットは "" (未分類)。

#include "ergo/package_model/package_model.h"

#include <string>
#include <vector>

namespace ergo::package_model {

struct LayerRule {
    std::string glob;
    std::string layer;
};

/// 宣言を読む (glob 順に並べて返す)。
bool read_layers_text(const std::string& json, std::vector<LayerRule>& rules,
                      std::string* error = nullptr);
bool read_layers_file(const std::string& path, std::vector<LayerRule>& rules,
                      std::string* error = nullptr);

/// glob が repo 相対パスに完全一致するか。
bool glob_match(const std::string& glob, const std::string& path);

/// repo 相対パスの層 ("" = どれにも当たらない)。 `rules` は glob 順。
std::string layer_of(const std::vector<LayerRule>& rules, const std::string& path);

/// `package == "ergo"` のターゲットに層を入れる (ソースは model.source_root 基準)。
void assign_layers(PackageModel& model, const std::vector<LayerRule>& rules);

} // namespace ergo::package_model
