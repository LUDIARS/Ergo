# ergo_package_manager モジュール定義

## 概要

パッケージマネージャ (spec/feature/package-manager.md §2、 段階 W1 「宣言と読み取り」) の読み取り層と、
その CLI `ergo_pkg`。 ホストの宣言 (ホスト側 `ergo-packages.json` の `host` 節、 形式は
spec/data/ergo-packages.md 「ホストの宣言」) を読んで検査し、 ブートストラップを生成し、 一覧を作る。
ライブラリはプロセスを起こさず、 ネットワークにも出ない (git / cmake の起動は CLI 側だけ)。

| 部品 | ヘッダ | 中身 |
|---|---|---|
| 検査結果 | `finding.h` | `<id>\|<severity>\|<subject>\|<message>` (構成時の findings と同じ形)、 STRICT の格上げ、 整列 |
| Ergo のマニフェスト | `ergo_manifest.h` | 名前・種類・取得元・pin と modules / observe の名前だけを読む。 `host` 節のあるファイルは拒む |
| ホストの宣言 | `host_declaration.h` | `host.code` / `ergo` / `use` / `optional` / `features` / `legacy_vars` と非公開 `packages`。 型違いは MANIFEST の error |
| 宣言の検査 | `declaration_check.h` | PKG-01 (知らない名前)、 PKG-02 (40 桁 SHA でない pin)、 PKG-15 (非公開名と Ergo の名前の衝突)、 MANIFEST (Ergo の pin が無い・`optional` が `use` に無い・機能指定の書式・`legacy_vars` の行き先) |
| 漏れ検査 | `leak_check.h` | §8 の形 (ドライブ文字始まり・`/Users/`・`/home/`・UNC・token / secret / authorization のキー)。 JSON は JSON Pointer、 テキストは行番号で場所を返す |
| ブートストラップ | `bootstrap.h` | ひな形 (`cmake/packages/ergo-bootstrap.cmake.in`) の生成元 SHA の差し込み、 生成元の読み取り、 古さ (Missing / Current / Stale / Foreign) |
| init の計画 | `init_plan.h` | 既存ファイルから書くものを決める純粋関数。 2 回目は無変更、 `.gitignore` に `.ergo/` を 1 行、 `--upm` は `.ergo/host.json` だけ |
| UPM の宣言 | `upm_declaration.h` | `.ergo/host.json` の code と `Packages/manifest.json` の LUDIARS git 依存 (`#main` などは `branch_ref`) |
| 一覧 | `package_list.h` | 宣言・Ergo のマニフェスト・resolved.json・UPM を 1 パッケージ 1 行に。 表と JSON |

## カテゴリ

システム

## 所属ドメイン

build-packages (ビルドとパッケージ取り込み。 パッケージ・ハブの `cmake/packages/` と同じ。 `spec/domains/build-packages.domain.json`、 CLI `tools/package_cli/` も同じ)

## 必要なデータ

- ホストの `ergo-packages.json` (`host` 節)、 `cmake/ergo-bootstrap.cmake`、 `.gitignore`、 `.ergo/host.json`、 `Packages/manifest.json`
- Ergo の `ergo-packages.json` と `cmake/packages/ergo-bootstrap.cmake.in`
- 任意: `<build>/ergo-packages.resolved.json` (`list --build`)

## 依存

- `ergo_common` (jsonm)。 新しい third-party は無い
- CLI は `ergo_package_manager` だけに依存する。 `ergo_package_model` / `ergo_package_viz` とは互いに import しない

向き: `tools/package_cli → ergo_package_manager → ergo_common`。

## CLI `ergo_pkg` (ターゲット `ergo_pkg_cli`、 `ERGO_BUILD_PACKAGE_CLI`、 置き場所 `tools/package_cli/`)

`main.cpp` は配線だけで、 コマンドごとに `cmd_<name>.cpp`。 終了コードは `ergo_package_viz` と同じ:
0 成功 / 1 要対応の検出あり / 2 引数・入力の誤り / 3 書けない・取れない。 コミット・ブランチ作成・PR はしない。

| コマンド | 何をするか | 書き換えるもの | ネットワーク |
|---|---|---|---|
| `init [--code <c>] [--upm]` | ホストの宣言 (Ergo の pin は Ergo の `git rev-parse HEAD`)・ブートストラップ・`.gitignore` の `.ergo/`。 `--upm` は `.ergo/host.json` だけ。 既存の `ergo-packages.json` が `host` 節を持たなければ終了 2 | ホスト | 無し |
| `list [--json] [--build <dir>]` | Ergo 自身 → 共有 → 非公開 → UPM の順の一覧 | 無し | 無し |
| `doctor [--strict] [--json]` | 宣言の検査、 ブートストラップの古さ (Stale は error、 Missing / Foreign は warning)、 漏れ検査 (宣言・ブートストラップ・`.ergo/host.json`)。 error があれば終了 1、 `--strict` は warning も error | 無し | 無し |
| `fetch [<name>...] [--build <dir> \| --cache <dir>]` | 名前 (無ければ `host.use`) を `cmake -P cmake/packages/ErgoPackagePrefetch.cmake` で事前取得。 置き場所は `--cache` → `--build` の `FETCHCONTENT_BASE_DIR` (無ければ `<build>/_deps`) → 環境変数 `ERGO_DEPENDENCY_CACHE_DIR` | キャッシュのみ | 要 |

共通: `--host <dir>` (既定 `.`)、 `--ergo <dir>` (既定は環境変数 `ERGO_SOURCE_DIR`、 次にこの CLI を作った Ergo)。
`fetch` の cmake は `--cmake <exe>` (既定はこの CLI を作った cmake)。 シェルに渡す値は引用符・`%`・`$`・`` ` ``・改行を拒む。

## 変数

| 変数 | 意味 |
|---|---|
| `ERGO_BUILD_PACKAGE_MANAGER` | モジュール (既定 ON) |
| `ERGO_BUILD_PACKAGE_CLI` | CLI (既定 ON、 モジュールが要る) |
| `ERGO_PACKAGES_HOST_ID` | 構成時: `host.code` が無いときの resolved.json の `host.id` (無ければ `CMAKE_PROJECT_NAME`) |

## 作業

- 入力: 上の「必要なデータ」
- 出力: 検査結果 (findings)、 生成するファイルの中身、 一覧
- タスク: W2 (add / remove / update の書式を保つ書き換え)、 W3 (ergo_package_watch) はこのモジュールの上に足す

## テスト

| テスト | 何を確かめるか |
|---|---|
| `test_package_manager` | 見本 3 種 (host のみ / 非公開 packages あり / 不正) の読み取りと拒否理由、 ホストの宣言を Ergo のマニフェストとして読むと誤り、 PKG-15、 漏れ検査の真偽、 ブートストラップの古さの真偽 (CRLF は同じとみなす)、 init の 2 回目が無変更・`.gitignore` に 1 行、 `--upm`、 一覧 |
| `test_packages_host_declaration` (`cmake -P`) | CMake の読み手: `host` 節のファイルを Ergo のマニフェストとして読むと FATAL_ERROR、 `HOST` で code と非公開パッケージ、 PKG-15、 旧来の追加マニフェスト |
| `test_packages_host_configure` | 構成: ホスト直下の宣言の自動検出、 `host.id` の 3 段の決め方、 PKG-15 で M0 でも停止、 `ERGO_PACKAGE_MANIFEST_EXTRA` |
| `test_packages_bootstrap` | 偽 Ergo + 偽ホストで `ERGO_SOURCE_DIR` → 旧変数の別名 → 兄弟の順、 取得 OFF ではネットワークに出ない (`_deps` を作らない)、 取得 ON でも pin が SHA でなければ PKG-02 |
| `test_packages_prefetch` | 事前取得が FetchContent の置き場所に pin で取る (2 回目は触らない)、 取得元を消した後の `ERGO_PACKAGES_FETCH=ON` 構成が経路 5 に当たる |
| `test_package_cli` | CLI を通しで: init 2 回、 Ergo のマニフェストへの init は終了 2、 `--upm`、 doctor の古さと漏れの真偽、 list、 非公開パッケージの fetch |

テストはネットワークを使わない (git はテストの中で作る偽リポ、 `file://` で参照)。 `test_packages_m0_invariance` は変えない。
