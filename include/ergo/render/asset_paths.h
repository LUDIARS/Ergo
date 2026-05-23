#pragma once

/// ergo_render — シェーダ / アセットルートの解決ユーティリティ。
///
/// ゲームの WorldRenderer が各自で抱えていた「実行ファイル近傍を上方探索して
/// アセットルートを決める」横断ロジックを共通化する。 実行ディレクトリ・別
/// ドライブ構成・CI など環境差を吸収するための薄いヘルパー。
///
/// 解決方針は両関数とも共通:
///   1. 環境変数による明示指定があればそれを最優先で使う。
///   2. 探索の起点ディレクトリ (既定は cwd) から、 指定ディレクトリ名を
///      上方向へ数階層辿って最初に見つかったものを採る。
///   3. いずれも無ければ、 起点直下のパスをフォールバックで返す
///      (存在保証は無い — 呼び出し側の open 失敗で縮退する想定)。
///
/// 戻り値は末尾スラッシュ無しの正規化済みディレクトリパス (generic 形式)。

#include <string>

namespace ergo::render {

/// シェーダディレクトリ (`*.spv` の置き場) を解決する。
///
/// 解決順:
///   1. 環境変数 `ERGO_RENDER_SHADER_DIR`
///   2. `start_dir` から上方向へ `dir_name` (既定 "shaders") を探索
///   3. フォールバック: `start_dir / dir_name`
///
/// `start_dir` を空にすると現在の作業ディレクトリを起点にする。
/// `max_levels` は上方探索する階層数。
std::string resolve_shader_dir(const std::string& dir_name   = "shaders",
                               const std::string& start_dir  = "",
                               int                max_levels = 5);

/// ゲームアセットのルートディレクトリを解決する。
///
/// 解決順:
///   1. 環境変数 `ERGO_RENDER_ASSET_ROOT`
///   2. `start_dir` から上方向へ `dir_name` を探索
///   3. フォールバック: `start_dir / dir_name`
///
/// `dir_name` はゲームごとに異なる (例: KS は "KzSUnity")。 `start_dir` を
/// 空にすると現在の作業ディレクトリを起点にする。
std::string resolve_asset_root(const std::string& dir_name,
                               const std::string& start_dir  = "",
                               int                max_levels = 5);

} // namespace ergo::render
