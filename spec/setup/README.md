# Ergo セットアップガイド (用途別)

Ergo は **C++17 モジュール群** (`ergo_<domain>`、`ergo_math` / `ergo_physics2d`
のみ C++20) と、それらに付随する **共有プラグインホスト型の開発者ツール**
(C++ ネイティブアプリ `ergo_studio`、`tools/studio`) を 1 リポジトリに集約したものです。「何をしたいか」 によって
触る場所と必要な設定が変わるため、このフォルダに用途別のセットアップガイドを
置いています。

リポジトリ全体の概要・運用方針は丸写しせず、ここでは
「○○するための最短手順 + 実在する設定キー」 だけをまとめます。詳細は各
リンク先 (README / CLAUDE / `spec/`) を参照してください。

## 用途別インデックス

| やりたいこと | ガイド | 主な設定 |
|---|---|---|
| **まず一周する** (clone → 例のビルド → Ergo Studio で値を変えて反映) | [quickstart.md](quickstart.md) | `ergo_studio` ターゲット / `ergo_example_compose_loop --connect` |
| C++ モジュール (ergo_input / ergo_render 等) をビルドする | [build-cpp.md](build-cpp.md) | CMake `ERGO_BUILD_*` / `ERGO_AUDIO_BACKEND` / `ERGO_PARTICLE_HAS_RENDERER` |
| 開発者ツール `ergo_studio` をビルドして起動する | [run-tools-ergo.md](run-tools-ergo.md) | `ERGO_BUILD_STUDIO` / `ERGO_PACKAGES=pictor` / `--headless` / `PORT` |
| 複数モジュールを自分のフレームループにつなぐ (呼び出し順・dt・型変換) | [integrate-modules.md](integrate-modules.md) | `examples/compose_loop` / `ERGO_BUILD_EXAMPLES` |
| ゲーム固有エディタを plugin pack として読み込む | [external-plugins.md](external-plugins.md) | `ERGO_PLUGIN_DIR` |
| 組み込みプラグイン (render_pipeline / visus) にホスト資産を見せる | [plugin-data-roots.md](plugin-data-roots.md) | `ERGO_PICTOR_PROFILE_DIR` / `VISUS_PROJECT_ROOTS` |
| 変更を main に入れる (ブランチ + PR 運用) | [branch-and-pr.md](branch-and-pr.md) | feat ブランチ + PR (main 直 push 禁止) |
| LUDIARS パッケージ (Pictor / Tela) の版の固定と記録 | [packages.md](packages.md) | `ergo-packages.json`、互換モード M0 |
| CI の各ジョブの内容と手元での再現 | [ci.md](ci.md) | GitHub Actions / CMake プリセット |
| 環境変数 / ビルドフラグ / ポートを一覧で見る | [config-reference.md](config-reference.md) | 全キー早見表 |

## 前提

- **C++ モジュール側**: CMake `>= 3.20`、C++17 対応コンパイラ。`ergo_math` /
  `ergo_physics2d` は C++20 で、リンクする側にも C++20 が伝わる。テストは
  CTest。詳細は [build-cpp.md](build-cpp.md)。
- **開発者ツール側 (`ergo_studio`、`tools/studio`)**: 同じ CMake ツールチェーン。
  ウィンドウ付きで動かすには Vulkan SDK と Pictor (`ERGO_PACKAGES=pictor`) が要り、
  無ければヘッドレス専用の exe になる。LUDIARS の環境では Excubitor の
  `ergo-studio` から起動する。詳細は [run-tools-ergo.md](run-tools-ergo.md)。
- どちらも **変更は feat ブランチ + PR**。main への直接 push は禁止
  (例外なし、[branch-and-pr.md](branch-and-pr.md))。

## 最短起動 / ビルド

```bash
# --- C++ モジュールをビルド + テスト ---
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release

# --- 開発者ツール (Ergo Studio) をビルドして起動 ---
cmake -S . -B build -DERGO_PACKAGES=pictor -DERGO_PACKAGES_FETCH=ON   # ウィンドウには Vulkan SDK が要る
cmake --build build --config Release --target ergo_studio
build/studio/ergo_studio.exe              # ウィンドウ付き (port 5170)
build/studio/ergo_studio.exe --headless   # サーバのみ
```

## 関連設計リンク

- リポジトリ概要・ディレクトリ構成: [`../../README.md`](../../README.md)
- AI / 自動化向け運用ルール (ブランチ運用・コード規約): [`../../CLAUDE.md`](../../CLAUDE.md)
- 開発者ツールの全体仕様 (プラグイン I/F・外部 plugin pack): [`../feature/tool/ergo.md`](../feature/tool/ergo.md)、
  ソースの説明: [`../../tools/studio/README.md`](../../tools/studio/README.md)
- 各 C++ モジュールの定義書: [`../feature/module/`](../feature/module/) と
  [`../module/`](../module/) の 2 か所に分かれている。どちらにあるかは
  [`../../module_list.md`](../../module_list.md) の「仕様書」列を参照
- モジュール一覧: [`../../module_list.md`](../../module_list.md)
