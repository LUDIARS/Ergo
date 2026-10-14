# ergo_package_manager モジュール定義

## 概要

パッケージマネージャ (spec/feature/package-manager.md §2、 段階 W1 「宣言と読み取り」・W2 「書き換え」) の
読み取り層・書き換え層と、 その CLI `ergo_pkg`。 ホストの宣言 (ホスト側 `ergo-packages.json` の `host` 節、 形式は
spec/data/ergo-packages.md 「ホストの宣言」) を読んで検査し、 ブートストラップを生成し、 一覧を作る。
W2 で宣言の書き換え (add / remove) と pin の書き換え (update) を足した。
W6 で P4 ホスト状態図の入力 `ergo-packages.status.json` の書き手 (テスト結果の集計と `ergo_pkg status`) を足した。
ライブラリはプロセスを起こさず、 ネットワークにも出ない (git / cmake の起動は CLI 側だけ。 版の解決は `IRevResolver` 越し)。

| 部品 | ヘッダ | 中身 |
|---|---|---|
| 検査結果 | `finding.h` | `<id>\|<severity>\|<subject>\|<message>` (構成時の findings と同じ形)、 STRICT の格上げ、 整列 |
| Ergo のマニフェスト | `ergo_manifest.h` | 名前・種類・取得元・pin と modules / observe の名前だけを読む。 `host` 節のあるファイルは拒む |
| ホストの宣言 | `host_declaration.h` | `host.code` / `ergo` / `use` / `optional` / `features` / `legacy_vars` と非公開 `packages`。 `track.branch` (W3) も読む。 型違いは MANIFEST の error |
| 宣言の検査 | `declaration_check.h` | PKG-01 (知らない名前)、 PKG-02 (40 桁 SHA でない pin)、 PKG-15 (非公開名と Ergo の名前の衝突)、 MANIFEST (Ergo の pin が無い・`optional` が `use` に無い・機能指定の書式・`legacy_vars` の行き先) |
| 漏れ検査 | `leak_check.h` | §8 の形 (ドライブ文字始まり・`/Users/`・`/home/`・UNC・token / secret / authorization のキー)。 JSON は JSON Pointer、 テキストは行番号で場所を返す |
| ブートストラップ | `bootstrap.h` | ひな形 (`cmake/packages/ergo-bootstrap.cmake.in`) の生成元 SHA の差し込み、 生成元の読み取り、 古さ (Missing / Current / Stale / Foreign) |
| init の計画 | `init_plan.h` | 既存ファイルから書くものを決める純粋関数。 2 回目は無変更、 `.gitignore` に `.ergo/` を 1 行、 `--upm` は `.ergo/host.json` だけ |
| UPM の宣言 | `upm_declaration.h` | `.ergo/host.json` の code と `Packages/manifest.json` の LUDIARS git 依存 (`#main` などは `branch_ref`) |
| 一覧 | `package_list.h` | 宣言・Ergo のマニフェスト・resolved.json・UPM を 1 パッケージ 1 行に。 表と JSON |
| 書式を保つ書き換え (W2) | `json_edit.h` | キーの経路で値の範囲を探し、 その範囲だけを差し替える。 足すメンバ・要素は兄弟の区切り (改行と字下げ / 1 行) を写し、 CRLF の文書には CRLF で書く |
| 持ち主 (W2) | `package_owner.h` | `ergo` → ホストの `host.ergo`、 非公開 → ホストの `packages.<name>`、 共有 → Ergo のマニフェスト。 非公開と Ergo の同名は Conflict (PKG-15) |
| 版の解決 (W2) | `rev_resolution.h` | `--to` をタグ (expect_tag つき) / 追跡ブランチ (main) / 40 桁 SHA / その他のブランチの順に解決。 タグの無い pin は STRICT の警告、 main の祖先でない commit は `allow_off_main` が無ければ拒否。 git は `IRevResolver` (CLI のミラー実装、 テストは偽物) |
| テスト結果 (W6) | `test_results.h` | ctest の `--output-junit` の XML (`<testcase>` を数え、 `<failure>` / `<error>` か status="fail" を失敗、 `<skipped>` か status="notrun" / "disabled" を未実行) と `Testing/Temporary/LastTest.log` (行頭の `Test Passed.` / `Test Failed` / `Test Not Run`) の集計。 テスト名・出力は持たない |
| ホストの状態 (W6) | `host_status.h` | `ergo-packages.status.json` (spec/data/ergo-packages-status.md) の JSON。 キーは辞書順の 1 行で、 同じ入力なら同じバイト列 |
| 宣言の書き換え (W2) | `declaration_edit.h` | `plan_add` / `plan_remove` / `plan_pin`。 書き換え後に宣言を検査し直し、 新しく error が出るものは拒む。 remove は `modules.*.needs` を壊すなら拒否、 非公開 link の add は結線ファイルが無ければ拒否 |

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
- CLI は `ergo_package_manager` と (W3 の `outdated` / `watch` で) `ergo_package_watch` に依存する。
  `ergo_package_model` / `ergo_package_viz` とは互いに import しない

向き: `tools/package_cli → (ergo_package_watch →) ergo_package_manager → ergo_common`。 W3 は [`package_watch.md`](package_watch.md)。

## CLI `ergo_pkg` (ターゲット `ergo_pkg_cli`、 `ERGO_BUILD_PACKAGE_CLI`、 置き場所 `tools/package_cli/`)

`main.cpp` は配線だけで、 コマンドごとに `cmd_<name>.cpp`。 終了コードは `ergo_package_viz` と同じ:
0 成功 / 1 要対応の検出あり / 2 引数・入力の誤り / 3 書けない・取れない。 コミット・ブランチ作成・PR はしない。

| コマンド | 何をするか | 書き換えるもの | ネットワーク |
|---|---|---|---|
| `init [--code <c>] [--upm]` | ホストの宣言 (Ergo の pin は Ergo の `git rev-parse HEAD`)・ブートストラップ・`.gitignore` の `.ergo/`。 `--upm` は `.ergo/host.json` だけ。 既存の `ergo-packages.json` が `host` 節を持たなければ終了 2 | ホスト | 無し |
| `list [--json] [--build <dir>]` | Ergo 自身 → 共有 → 非公開 → UPM の順の一覧 | 無し | 無し |
| `doctor [--strict] [--json]` | 宣言の検査、 ブートストラップの古さ (Stale は error、 Missing / Foreign は warning)、 漏れ検査 (宣言・ブートストラップ・`.ergo/host.json`)。 error があれば終了 1、 `--strict` は warning も error | 無し | 無し |
| `add <name> [--optional] [--features <f>] [--kind link\|tool] [--code <c>] [--repository <url> --rev <sha\|tag>] [--wiring <path>]` | `host.use` (と optional / features) に足す。 Ergo のマニフェストに無い名前は非公開 `packages` に足し、 `--repository` / `--rev` が要る (`--rev` がタグ・ブランチならミラーで解決、 40 桁ならネットワークに出ない)。 link は `cmake.wiring` (既定 `cmake/packages/<name>.cmake`) がホストに無ければ終了 2 でファイル不変。 共有パッケージに `--repository` などを渡すと終了 2 | ホスト | 任意 |
| `remove <name>` | `host.use` / `optional` / `features` と非公開 `packages` (とそれを指す `legacy_vars`) から外す。 Ergo の `modules.*.needs` にある名前は終了 2。 宣言に無ければ無変更 | ホスト | 無し |
| `update <name> --to <tag\|main\|sha\|branch> [--allow-off-main] [--cache <dir>]` | 40 桁 SHA に解決して持ち主のファイルの `rev` と `expect_tag` だけを書く (タグでなければ `expect_tag` を外す)。 共有パッケージは `--ergo` を明示したときだけ Ergo のマニフェストを書き、 無ければ終了 2。 `ergo` と非公開はホストの宣言 (無ければ終了 2)。 off-main は終了 2 (`--allow-off-main` で通す)。 最後に STRICT で通るかと次の操作 (どのリポで PR か) を表示する | 持ち主 | 要 |
| `status --build <dir> [--junit <xml>] [--out <file>]` (W6) | P4 の入力 `<build>/ergo-packages.status.json` (`--out` で別の場所) を書く: ホストのコード (宣言の `host.code` → resolved.json の `host.id`)、 `git rev-parse HEAD`、 `CMakeCache.txt` の更新時刻 (UTC)、 テストの数 (`--junit` → `<build>/Testing/Temporary/LastTest.log` → `unknown`)。 ビルドの成否と CI の結果は取らない (§9 W-6)。 読めない `--junit`・`--build` 無しは終了 2、 漏れ検査に当たれば書かずに終了 3 | ビルドディレクトリ | 無し |
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
- タスク: W2 (add / remove / update の書式を保つ書き換え) は実装済み (`spec/tasks/2026-10-15-package-manager-w2.md`)。 W3 (ergo_package_watch) はこのモジュールの上に足す

## テスト

| テスト | 何を確かめるか |
|---|---|
| `test_package_manager` | 見本 3 種 (host のみ / 非公開 packages あり / 不正) の読み取りと拒否理由、 ホストの宣言を Ergo のマニフェストとして読むと誤り、 PKG-15、 漏れ検査の真偽、 ブートストラップの古さの真偽 (CRLF は同じとみなす)、 init の 2 回目が無変更・`.gitignore` に 1 行、 `--upm`、 一覧 |
| `test_packages_host_declaration` (`cmake -P`) | CMake の読み手: `host` 節のファイルを Ergo のマニフェストとして読むと FATAL_ERROR、 `HOST` で code と非公開パッケージ、 PKG-15、 旧来の追加マニフェスト |
| `test_packages_host_configure` | 構成: ホスト直下の宣言の自動検出、 `host.id` の 3 段の決め方、 PKG-15 で M0 でも停止、 `ERGO_PACKAGE_MANIFEST_EXTRA` |
| `test_packages_bootstrap` | 偽 Ergo + 偽ホストで `ERGO_SOURCE_DIR` → 旧変数の別名 → 兄弟の順、 取得 OFF ではネットワークに出ない (`_deps` を作らない)、 取得 ON でも pin が SHA でなければ PKG-02 |
| `test_packages_prefetch` | 事前取得が FetchContent の置き場所に pin で取る (2 回目は触らない)、 取得元を消した後の `ERGO_PACKAGES_FETCH=ON` 構成が経路 5 に当たる |
| `test_package_manager` (W2) | 書式を保つ書き換え (値だけの差し替え、 兄弟の区切りの写し、 空のコンテナ、 CRLF、 文字列中の括弧)、 持ち主の判定、 `update --to <tag>` が rev と expect_tag だけを書く (バイト一致)、 expect_tag の追加と除去、 add (共有 / 非公開 link の結線 / 拒否する書き換え)、 remove (needs の拒否、 非公開の除去、 add と remove の往復でバイト一致)、 版の解決 (タグ・main・SHA・off-main の拒否と許可・短縮 SHA・取れない) |
| `test_package_cli_edit` (`cmake -P`) | 偽リポ (file://) で CLI を通しで: add (共有)、 update の持ち主 (共有は `--ergo` 側、 無ければ終了 2、 ホストの宣言でない `--host` で終了 2)、 off-main の拒否と `--allow-off-main`、 `--to main` の STRICT 警告、 `update ergo`、 remove の needs 拒否 (ファイル不変)、 結線ファイルの無い add (終了 2・ファイル不変) と非公開パッケージのタグの解決、 同じ pin への update は無変更 |
| `test_package_manager` (W6) | JUnit / LastTest.log の数え方 (`<testcases>` は数えない、 error 子、 行頭だけ)、 status.json が P4 の見本とバイト一致 (JUnit 見本から)、 未実行は数を書かない、 漏れ無し |
| `test_package_host_view` (`cmake -P`) | `ergo_pkg status` (JUnit → LastTest.log → unknown、 コードは resolved.json から、 作業ツリーのパスを書かない、 読めない `--junit` と `--build` 無しは終了 2) → `ergo_package_viz --view host` (見本で書ける、 同じ入力で同じバイト列、 入力が全部無くても `—` で書く) |
| `test_package_cli` | CLI を通しで: init 2 回、 Ergo のマニフェストへの init は終了 2、 `--upm`、 doctor の古さと漏れの真偽、 list、 非公開パッケージの fetch |

テストはネットワークを使わない (git はテストの中で作る偽リポ、 `file://` で参照)。 `test_packages_m0_invariance` は変えない。
