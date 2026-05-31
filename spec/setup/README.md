# Ergo セットアップガイド (用途別)

Ergo は **C++17 モジュール群** (`ergo_<domain>`) と、それらに付随する
**共有プラグインホスト型 Web 開発者ツール** (`tools/ergo`) を 1 リポジトリに
集約したものです。「何をしたいか」 によって触る場所と必要な設定が変わるため、
このフォルダに用途別のセットアップガイドを置いています。

リポジトリ全体の概要・運用方針は丸写しせず、ここでは
「○○するための最短手順 + 実在する設定キー」 だけをまとめます。詳細は各
リンク先 (README / CLAUDE / `spec/`) を参照してください。

## 用途別インデックス

| やりたいこと | ガイド | 主な設定 |
|---|---|---|
| C++ モジュール (ergo_input / ergo_render 等) をビルドする | [build-cpp.md](build-cpp.md) | CMake `ERGO_BUILD_*` / `ERGO_AUDIO_BACKEND` / `ERGO_PARTICLE_HAS_RENDERER` |
| 開発者ツール `tools/ergo` を起動する | [run-tools-ergo.md](run-tools-ergo.md) | `npm start` (Electron) / `npm run dev` / `PORT` |
| ゲーム固有エディタを plugin pack として読み込む | [external-plugins.md](external-plugins.md) | `ERGO_PLUGIN_DIR` |
| 組み込みプラグイン (render_pipeline / visus) にホスト資産を見せる | [plugin-data-roots.md](plugin-data-roots.md) | `ERGO_PICTOR_PROFILE_DIR` / `VISUS_PROJECT_ROOTS` |
| 変更を main に入れる (ブランチ + PR 運用) | [branch-and-pr.md](branch-and-pr.md) | feat ブランチ + PR (main 直 push 禁止) |
| 環境変数 / ビルドフラグ / ポートを一覧で見る | [config-reference.md](config-reference.md) | 全キー早見表 |

## 前提

- **C++ モジュール側**: CMake `>= 3.16`、C++17 対応コンパイラ。テストは
  CTest。詳細は [build-cpp.md](build-cpp.md)。
- **Web ツール側 (`tools/ergo`)**: Node `>= 20` (`package.json` の `engines`)。
  Electron スタンドアロン起動が既定。詳細は [run-tools-ergo.md](run-tools-ergo.md)。
- どちらも **変更は feat ブランチ + PR**。main への直接 push は禁止
  (例外なし、[branch-and-pr.md](branch-and-pr.md))。

## 最短起動 / ビルド

```bash
# --- C++ モジュールをビルド + テスト ---
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release

# --- 開発者ツールを起動 (Electron スタンドアロン) ---
cd tools/ergo
npm install
npm start            # ヘッドレスは npm run dev (port 5170)
```

## 関連設計リンク

- リポジトリ概要・ディレクトリ構成: [`../../README.md`](../../README.md)
- AI / 自動化向け運用ルール (ブランチ運用・コード規約): [`../../CLAUDE.md`](../../CLAUDE.md)
- 開発者ツールの全体仕様 (プラグイン I/F・シェル拡張): [`../tool/ergo.md`](../tool/ergo.md)
- 各 C++ モジュールの定義書: [`../module/`](../module/)
- モジュール一覧: [`../../module_list.md`](../../module_list.md)
