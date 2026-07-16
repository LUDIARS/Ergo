# ergo_resource モジュール定義

> 初版: 2026-07-17。MUSA Clio (UnityFoundation `spec/tools/Clio.md`) の
> タグ駆動リソース自動選択を、Unity 非依存の C++ 実装として Ergo に移植する。

## 概要

タグでリソース (ファイル) を自動選択して読み込むモジュール。ホストアプリは
「欲しいタグ + 名前ヒント」を渡すだけで、カタログ (JSON マニフェスト) に
登録されたリソースからスコアリングで最良候補が選ばれ、バイト列として
ロードされる。プレースホルダ (キューブ等) に付けたタグから実アセットを
解決する Clio と同じ発想の、エンジン側ランタイム。

選択ロジック (スコアラ) は Clio と同一の重み付け (タグ一致 +10、名前完全
一致 +5、名前トークン +2) で、タグ語彙は Curare の `assets.tags` と共有する
前提。Curare との同期はマニフェスト生成側 (ツール / パイプライン) の責務で、
本モジュールはネットワークに出ない (オフラインで完結)。

デコード (画像 / モデル / 音) は行わない。`LoadedResource::bytes` を受けた
ホスト側が各デコーダに渡す。

## カテゴリ

システム

## 所属ドメイン

アセット / リソース管理

## 必要なデータ

- リソースマニフェスト JSON (`resources.json` 等):
  `{"resources":[{"id":"tree01","name":"tree01","path":"models/tree01.glb","tags":["tree","nature"]}]}`
  - `path` 必須 (相対パスはマニフェストのあるディレクトリ基準で解決)
  - `id` 省略時は `path`、`name` 省略時はファイル名 (拡張子なし)
  - `tags` 省略可 (空)
- リソース実体ファイル (任意形式、バイト列として読む)

## 依存

- C++17: `<string>` `<vector>` `<algorithm>` `<cctype>` `<filesystem>`
- Ergo: `ergo_io` (ファイル読み込み)、`ergo_common` (`jsonm` マニフェスト解析)
  — `ergo_ui_kit` / `ergo_ui_layout` と同じ下層依存構成
- テスト: mini-gtest (`ergo_gtest_main`)

## 変数

- `Catalog::entries_` — 読み込んだ `Entry` (id / name / path / tags) の列
- `Catalog::last_error_` — 直近の load 失敗理由 (握りつぶし禁止のため文字列で保持)

## 作業

### 入力

- `Catalog::load_manifest(path)` — マニフェスト追記ロード (複数回可)
- `Catalog::add(entry)` / `clear()`
- `resolve_candidates(catalog, wanted_tags, name_hint, max)` /
  `resolve_best(...)` — スコア降順の候補列挙 / 最良 1 件
- `load(entry, out)` / `resolve_and_load(catalog, wanted_tags, name_hint, out)`

### 出力

- `Candidate{entry*, score}` の列 (score <= 0 は除外)
- `LoadedResource{entry, bytes}` — 解決済みエントリ + ファイル内容
- 失敗は `bool false` + `Catalog::last_error()` (ロード系) で観測可能

### タスク (実装分解)

- [x] R1: 本 spec (設計宣言 + タスク分解)
- [x] R2: `Entry` 構造体 (`entry.h`)
- [x] R3: `tag_scorer` — Clio と同重みの純ロジック (+ 名前ヒント抽出)
- [x] R4: `Catalog` — マニフェスト JSON ロード / 相対パス解決 / 語彙列挙
- [x] R5: `resolver` — 候補列挙 + 最良解決
- [x] R6: `loader` — バイト列ロード + resolve_and_load 複合
- [x] R7: テスト (scorer / catalog / resolver+loader、実ファイル読込を含む)
- [x] R8: CMake 統合 (`ERGO_BUILD_RESOURCE`) + module_list.md / .yaml + README
- [ ] R9: ローカルビルド + ctest green 確認 → PR → CI green

## テスト

- スコアラ: タグ一致数 / 名前完全一致 / トークン部分一致 / 大文字小文字非依存 /
  プレースホルダ既定名 (`Cube (1)`) はヒント無し扱い
- カタログ: 正常ロード / 既定値補完 (id・name) / 相対パス解決 / 語彙の
  ソート済みユニーク性 / 壊れた JSON・欠落ファイルで false + last_error
- リゾルバ: タグ優先の順位付け / 名前ヒントの補助加点 / 該当なしで nullptr /
  max_candidates 制限
- ローダ: 実ファイル (テスト内で生成) のバイト列一致 / 欠落ファイルで false

## 非対象 (将来)

- ディレクトリ走査によるカタログ自動生成 (マニフェスト生成ツール側)
- Curare API との直接同期 (`ergo_http` は既定 OFF。必要になったら
  マニフェスト生成ツールを `tools/ergo` プラグインとして追加する)
- 型別デコード (画像 / モデル / 音) — ホスト側デコーダの責務
