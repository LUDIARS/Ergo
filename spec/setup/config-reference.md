# 設定リファレンス (環境変数 / ビルドフラグ / ポート)

Ergo で実在する環境変数・CMake ビルドフラグ・ポートの早見表。各キーの詳細は
リンク先の用途別ガイドを参照。出典ファイルを併記する。

## 環境変数

| 変数 | 既定 | 対象 | 役割 | 出典 |
|---|---|---|---|---|
| `PORT` | `5170` | tools/ergo | Web ツールの listen ポート | `tools/ergo/src/main.ts:10` / `electron/main.cjs:17` |
| `ERGO_PLUGIN_DIR` | (なし) | tools/ergo | 外部 (ゲーム固有) plugin pack のディレクトリ。OS パス区切りで複数可 | `tools/ergo/src/core/external.ts:63` |
| `ERGO_PICTOR_PROFILE_DIR` | `../../../Pictor/profiles` → `./profiles` | tools/ergo (`render_pipeline`) | `*.profile.json` の場所 | `tools/ergo/src/plugins/render_pipeline/profile_store.ts:31` |
| `VISUS_PROJECT_ROOTS` | `process.cwd()` | tools/ergo (`visus`) | 描画定義のプロジェクトルート。`;` 区切りで複数可 | `tools/ergo/src/plugins/visus/index.ts:47` |

> 上記 4 つが `tools/ergo/src/` 配下で `process.env` から読まれる全変数。

## CMake ビルドフラグ

正本は [`../../CMakeLists.txt`](../../CMakeLists.txt)。`-D<KEY>=<値>` で指定。

### 全体

| キー | 既定 | 役割 |
|---|---|---|
| `ERGO_BUILD_TESTS`      | ON  | モジュールテストをビルド (CTest 登録) |
| `ERGO_BUILD_DUMMY`      | ON  | dummy プラグライブラリをビルド |
| `ERGO_BUILD_BENCHMARKS` | OFF | マイクロベンチをビルド (opt-in) |

### モジュール個別 (`ERGO_BUILD_<名>`、すべて既定 ON)

`INPUT` / `PARTICLE` / `GPU_PARTICLE` / `BIND` / `ACTOR` / `SOUND` / `FRAME` /
`PROFILE` / `LOG` / `IO` / `AUDIO` / `WORLD_TIME` / `BLACKBOARD` / `UI` /
`CUSTOS` / `HEALTH` / `SCORE` / `COMBO_COUNTER` / `TIMING_JUDGE` / `UI_KIT` /
`SHURIKEN_MIGRATOR` / `RENDER`

### モジュール固有ノブ

| キー | 既定 | 役割 |
|---|---|---|
| `ERGO_AUDIO_BACKEND`                | `auto` | `ergo_audio` バックエンド: `auto` / `fmod` / `dummy` |
| `ERGO_PARTICLE_HAS_RENDERER`        | OFF    | `ergo_particle` の Vulkan ビルボード描画 (pictor + Vulkan 必須) |
| `ERGO_GPU_PARTICLE_COMPILE_SHADERS` | ON     | compute シェーダを `glslc` で SPIR-V に bake |

### ビルド時に定義されるマクロ (フラグではなく自動分岐)

| マクロ | 条件 |
|---|---|
| `ERGO_RENDER_HAS_VULKAN=1` | `pictor` ターゲット存在 + `find_package(Vulkan)` 成功時に `ergo_render` へ定義 (`CMakeLists.txt:731`) |

CMake 必須要件: バージョン `>= 3.16`、C++17、`Threads`
(`CMakeLists.txt:1-6,50`)。

## ポート

| ポート | 用途 | 上書き |
|---|---|---|
| `5170` | tools/ergo シェル + 全プラグイン (HTTP / WS) | `PORT` 環境変数 |

> 旧 `particle-editor` (5173) / `variable-editor` (5174) は廃止済みで、すべて
> 5170 に統合 ([`../tool/ergo.md`](../tool/ergo.md) 「互換性」)。

## 関連ガイド

- C++ ビルド: [build-cpp.md](build-cpp.md)
- ツール起動: [run-tools-ergo.md](run-tools-ergo.md)
- 外部プラグイン: [external-plugins.md](external-plugins.md)
- プラグイン資産ルート: [plugin-data-roots.md](plugin-data-roots.md)
- ブランチ + PR 運用: [branch-and-pr.md](branch-and-pr.md)
