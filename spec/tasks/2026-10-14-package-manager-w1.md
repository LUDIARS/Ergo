# パッケージマネージャ W1 — 宣言と読み取り

- 日付: 2026-10-14
- タスク参照: `actio:3bfd8b79-adb2-4de2-a7e9-fc75dd26e12b` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-manager.md`](../feature/package-manager.md) §2.1 / §2.2 (init / list / doctor / fetch) / §2.3 / §7 の W1 行と受入条件 / §8
- 前提: [`spec/feature/package-hub.md`](../feature/package-hub.md) (S1〜S3 実装済み)
- ブランチ: `feat/package-manager-w1`

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | ホストの宣言 (`host` 節) のスキーマ。`host` 節のファイルを Ergo のマニフェストとして読むと構成エラー、`HOST` で読むと code と非公開 packages | `cmake/packages/ErgoPackageManifest.cmake`、`spec/data/ergo-packages.md` |
| 2 | ハブがホストの宣言を読む: `ERGO_PACKAGE_MANIFEST_EXTRA`、無ければ `host` 節つきのホスト直下 `ergo-packages.json`。非公開名の衝突は PKG-15 (M0 でも停止)。resolved.json に `host.id` (`host.code` → `ERGO_PACKAGES_HOST_ID` → `CMAKE_PROJECT_NAME`) | `cmake/packages/{ErgoPackages,ErgoPackageReport}.cmake` |
| 3 | `ergo_package_manager`: 宣言の読み取りと検査 (PKG-01 / 02 / 15、MANIFEST)、漏れ検査 (§8)、ブートストラップの生成と古さ、init の計画、UPM の宣言、一覧 | `include/ergo/package_manager/`、`src/package_manager/`、`spec/module/package_manager.md` |
| 4 | CLI `ergo_pkg` (ターゲット `ergo_pkg_cli`、`ERGO_BUILD_PACKAGE_CLI`): `main.cpp` は配線だけ、コマンドごとに 1 ファイル | `tools/package_cli/` |
| 5 | ブートストラップのひな形 (`ERGO_SOURCE_DIR` → 兄弟 → 取得 (明示 ON のみ)、先頭に生成元 SHA) | `cmake/packages/ergo-bootstrap.cmake.in` |
| 6 | 事前取得: FetchContent と同じ置き場所へ。経路 5 は pin と同じ HEAD の取得済みソースをそのまま使う (取得エンジンは FetchContent だけ) | `cmake/packages/{ErgoPackagePrefetch,ErgoPackageFetchCache}.cmake`、`ErgoPackageResolve.cmake` |
| 7 | テスト (偽リポはテスト内で git init、ネットワーク無し) | `tests/package_manager/`、`tests/packages/test_{host_declaration,host_configure,bootstrap,prefetch,package_cli}.cmake`、`tests/packages/pkg_git_support.cmake`、`tests/packages/fixtures/{decl_host,boot_ergo,boot_host}/`、`test_report.cmake` に `host.id` |
| 8 | 文書・登録: モジュール一覧・README・ドメイン・層・手順・統合設計の実装状況 | `module_list.md` / `.yaml`、`README.md`、`spec/domains/build-packages.domain.json` (追記)、`.anatomia/layers.json` (追記)、`spec/setup/packages.md`、`spec/feature/package-manager.md` |

## 受け入れ条件 (契約書式)

- C-1 read_host_declaration / check_declaration(host, ergo): 見本 3 種 (host のみ / 非公開 packages あり / 不正) を読み、不正には PKG-02 / PKG-01 / PKG-15 / MANIFEST の理由を返す — `test_package_manager` (PackageManagerDeclaration.*)
- C-2 ergo_pkg_manifest_load(file): `host` 節のファイルを Ergo のマニフェストとして読むと FATAL_ERROR (構成エラー)、C++ の read_ergo_manifest も拒む — `test_packages_host_declaration`、`test_packages_host_configure` (as_manifest)、`test_package_manager` (HostDeclarationIsNotAnErgoManifest)
- C-3 ergo_pkg_manifest_merge(ergo, host): 非公開名と Ergo のマニフェストの同名で PKG-15、構成は M0 でも停止 — `test_packages_host_declaration`、`test_packages_host_configure` (clash)
- C-4 plan_init / `ergo_pkg init`: 2 回目は無変更 (バイト一致)、`.gitignore` に `.ergo/` が 1 行、`--upm` は `.ergo/host.json` だけ — `test_package_manager` (PackageManagerInit.*)、`test_package_cli`
- C-5 ergo-bootstrap.cmake.in: 偽 Ergo + 偽ホストで `ERGO_SOURCE_DIR` → 兄弟の順に解決し、取得 OFF では FetchContent を起こさない — `test_packages_bootstrap`
- C-6 check_bootstrap / leak_check / `ergo_pkg doctor`: ブートストラップの古さと漏れ (§8) を真偽両方で検出 — `test_package_manager` (PackageManagerBootstrap.* / PackageManagerLeak.*)、`test_package_cli`
- C-7 ErgoPackagePrefetch.cmake / `ergo_pkg fetch`: 事前取得後の `ERGO_PACKAGES_FETCH=ON` 構成が取得元無しで経路 5 に当たる — `test_packages_prefetch`、`test_package_cli`
- C-8 resolved.json の `host.id` が `host.code` → `ERGO_PACKAGES_HOST_ID` → `CMAKE_PROJECT_NAME` — `test_packages_report`、`test_packages_host_configure`
- C-9 M0 の結果不変 — `test_packages_m0_invariance` (無変更で継続)

Augur の契約 (`augur.contracts.json` と述語モジュール) は用意できない。Augur の `contract-wrap` は TypeScript の関数を
包む仕組みで、C++ / CMake は対象にならない。上の受入条件は ctest のテスト名で裏づける。

## 再利用探索の採否

| 候補 | 採否 | 理由 |
|---|---|---|
| `ergo_common::jsonm` | 採用 | 宣言・マニフェスト・resolved.json・UPM の読み取りと `--json` の出力。新しい third-party は入れない |
| `ergo_package_model` の `json_access` / resolved リーダ | 不採用 | モジュール間の cross-import 禁止。resolved.json は一覧で `route` / `state` を読むだけなので jsonm で直接読む |
| `cmake/packages/ErgoPackageManifest.cmake` / `ergo_pkg_manifest_merge` / `ergo_pkg_rule_collisions` | 採用 | 構成時の読み手を `HOST` で拡張し、PKG-15 は既存の衝突規則のまま。事前取得スクリプトも同じ読み手を使う |
| 構成時の経路 5 (`_ergo_pkg_fetch` → `_ergo_fc_declare` → FetchContent) | 採用 | 事前取得は FetchContent を同じ置き場所で起こすだけ。経路 5 は取得済みソースを既存の `ERGO_SOURCE_DIR_<DEP>` の仕組みで渡す (エンジンは 1 つ) |
| `CMakeUserPresets.json` に変数を書く案 (Opus 素案) | 不採用 | 統合設計 §0 で Fable 案 (FetchContent と同じ置き場所) に決定済み |
| `tools/package_viz/version_probe.cpp` のプロセス起動 | 写して採用 | CLI 同士で共有ライブラリを作らず、同じ引用規則 (cmd /c の外側引用・危険文字の拒否) を `tools/package_cli/process.cpp` に持つ |
| `tests/packages/test_hub_routes.cmake` の git 補助 | 共通化して採用 | 既存ファイルは触らず、W1 のテスト用に `pkg_git_support.cmake` へ切り出した |

## 検証

| 何を | 結果 |
|---|---|
| MSVC (VS 2022, x64, Debug) で構成し `ergo_package_manager` / `ergo_pkg_cli` / `test_package_manager` をビルド | 成功 (警告は既存の `json_min.h` の C4819 のみ)。構成 (M0) で resolved.json に `"host": { "id": "Ergo" }` |
| テストの実行 (ctest、`cmake -P` のテストを含む) | 未実施 (委託の指示で、明示の指示が無い限りテストを実行しない) |
| GCC でのビルド | 未実施。designated initializer を使わず、`<cctype>` は `unsigned char` に揃え、popen / _popen はマクロで切り替えた |
| 秘匿語の辞書照合・絶対パス | 提出前にファイルとコミットで 0 件を確認 |

## 復旧

- CLI だけ止める: `-DERGO_BUILD_PACKAGE_CLI=OFF`。モジュールごと止める: `-DERGO_BUILD_PACKAGE_MANAGER=OFF`
  (既存モジュールはどれも依存しない)。
- 構成時の変化は 3 つ: (1) `host` 節のファイルを Ergo のマニフェストとして読むと止まる、(2) ホスト直下の `host` 節つき
  `ergo-packages.json` を自動で読む (今そのファイルを持つホストは無い)、(3) resolved.json の `host`。いずれもこの PR の
  revert で元に戻る。自動検出だけ避けたいホストは `ERGO_PACKAGE_MANIFEST_EXTRA` で別の (空の) 宣言を指せる。
- 経路 5 の取得済みソースの再利用を止める: `<FETCHCONTENT_BASE_DIR>/ergo_pkg_<name>-src` を消すか、
  `FETCHCONTENT_SOURCE_DIR_ERGO_PKG_<NAME>` で明示する (明示が常に優先)。

## 残り (W1 の外)

- `use` / `optional` / `features` を構成の既定値 (`ERGO_PACKAGES` など) として流すこと (Ludellus-Native の L2 で要る)
- Ergo 自身への PKG-04 / 05 / 07 (ブートストラップは PKG-02 だけ。drift の記録は resolved.json の拡張と合わせて後段)
- W2 以降 (add / remove / update、watch、Unity)
