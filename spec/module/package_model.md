# ergo_package_model モジュール定義

## 概要

パッケージ・ハブ (spec/feature/package-hub.md §5) の可視化と宣言漏れ検査の入力を 1 つの値型
`PackageModel` にまとめる読み取り層。 書き換えはしない。

| 入力 | 読むもの |
|---|---|
| `<build>/ergo-packages.resolved.json` (schema 1、 必須) | パッケージ (link / tool) の pin・HEAD・drift・dirty・経路・機能・`requires`・`group`、 モジュール、 observe、 findings |
| CMake File API codemodel v2 (`<build>/.cmake/api/v1/reply/`) | ターゲットの名前・型・ソース・直接の依存・定義したディレクトリ・project |
| `.anatomia/layers.json` (任意) | Ergo のターゲットの層 (P2 のグループ) |

File API は最新の `index-*.json` (名前順で最後) のクライアント `client-ergo` の応答を使い、 無ければ index の
`objects` の codemodel。 `version.major` が 2 でなければ明示エラー。 生成器が足すターゲット (`isGeneratorProvided`) と
`UTILITY` は読まない。

### 帰属

ターゲットを定義したディレクトリが最も長く一致する根へ帰属する。 根は link パッケージの `source_dir`、 Ergo
(`project(Ergo)` のディレクトリ)、 host (トップのソース。 Ergo がトップなら無い)。 ビルドツリーがソースの下にあっても、
その中 (FetchContent の `_deps` など) は ergo / host の根に数えない。 どの根にも入らないものは、 project 名 (小文字) が
既知のパッケージならそれ、 ソースの外 (またはビルドツリーの中) なら第三者 (project 名を小文字にしたパッケージ)、
それ以外は host。

### 既定の除外

テスト (`test_` で始まる名前、 またはソースがすべて `tests/` の下)、 ダミー (`_dummy` を含む)、 ベンチ (`bench_`、
`benchmarks/`)、 `ergo_gtest_main`。 図と PKG-17 は既定で見ない (`--include-tests` で戻す)。

### 層

`.anatomia/layers.json` の glob を Ergo のリポ相対パスへ完全一致で当てる (`*` = 1 階層、 `**` = 任意階層、 glob 順に
並べて先勝ち)。 ターゲットの層はソースの多数決 (同数は層の名前順)。 host と第三者には当てない。

### PKG-17 宣言漏れ

File API の直接の依存のうち、 「Ergo または link パッケージのターゲット → 別の link パッケージのターゲット」 で、
宣言 (Ergo 側はターゲット名に最長一致するモジュール (`<module>` か `<module>_…`) の `needs` / `optional`、 link 側は
`requires`) に無いもの。 構成時には判定しない (File API の返答は generate の後にしかできない)。

### 版の札

`VersionLabel{text, source}`。 resolved.json の `version` (project(VERSION)) を出所 `cmake` として持ち、 CLI が
`revisor version show` / `git describe --tags --long` の結果で置き換える (`version_from_revisor` /
`version_from_describe` は出力を札の形に直す純粋関数。 `v2.2.0-12-g…` → `2.2.0+12`)。

### jsonm の確認

`ergo_common::jsonm` で resolved.json・File API・layers.json を読めることを確かめた (オブジェクト・配列・文字列・
数値・真偽・null、 深さ 64 まで)。 足りなかったのは UTF-16 サロゲートペア (`\uD83D\uDE00` の 2 つ組) の復号だけで、
読み取り専用の拡張として `json_min.cpp` に足した (4 バイトの UTF-8 にする。 対にならない片割れは従来どおり)。
新規の third-party は入れていない。

## カテゴリ

システム

## 所属ドメイン

structure-visualization (`spec/domains/structure-visualization.domain.json`、 2026-10-14 の切り直しで observability から移動)。 層は infrastructure。

## 必要なデータ

構成済みのビルドディレクトリ (パッケージ・ハブが resolved.json と File API query を置く)。

## 依存

- `ergo_common` (jsonm)。 Tela / Pictor / tela_graph には依存しない。

## 変数

`LoadOptions`: `build_dir` / `resolved_path` (既定 `<build>/ergo-packages.resolved.json`) / `layers_path`
(既定 `<ergo_root>/.anatomia/layers.json`) / `require_codemodel` / `codemodel.configuration` (複数構成の生成器)。

## 作業

### 入力

上表の 3 種のファイル。

### 出力

`PackageModel` (パッケージ・モジュール・observe・findings・ターゲット)。 `find_undeclared_links` の結果。

### タスク

1. `resolved_reader` — schema 1 の検査と読み取り (observe はパッケージとしても載せる)。
2. `file_api_reader` — index・codemodel・target の読み取り、 version 検査、 パスの正規化。
3. `attribution` — 帰属と既定の除外、 ergo / host / 第三者のパッケージを足す。
4. `layers` — glob 照合と層の多数決。
5. `declaration_check` — PKG-17。
6. `version_label` — 版の札の文字列。
7. `loader` — 上をまとめる入口 (File API が無ければ注記を残して飛ばす)。

## テスト

`tests/package_model/test_package_model.cpp` と同梱の合成見本 `tests/package_model/fixtures/`
(`fileapi_host/` — host・Ergo・Pictor・ビルドツリー内の第三者・テスト・ダミー・生成器ターゲット、 古い index、
`fileapi_v3/` — 未対応の major、 `layers.json`)。 見本のパスは `/work/...` の架空のもので、 開発機のパスは含まない。
サロゲートペア、 resolved の読み取りと schema 拒否、 最新 index と生成器ターゲットの除外、 version 拒否と返答無し、
帰属 (ビルドツリー内の第三者を含む)、 既定の除外、 glob、 層、 PKG-17 (テストを戻した場合と link の `requires`)、
File API 無しの読み込み、 版の札、 パスの正規化。
