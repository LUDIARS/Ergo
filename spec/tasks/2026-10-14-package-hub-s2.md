# パッケージ・ハブ S2 — 取り込み (6 経路・M1 / M2・STRICT)

- 日付: 2026-10-14
- タスク参照: `actio:d0abfb44-da35-4684-829c-e6b4394eb787` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-hub.md`](../feature/package-hub.md) §2 上書き変数 / §3 取り込み / §4 検査 / §6 移行モード / §7 の S2 行
- 前段: [`2026-10-14-package-hub-s1.md`](2026-10-14-package-hub-s1.md) (main 取込済み)
- ブランチ: `feat/package-hub-s2`

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | 6 経路 (ホスト供給の HEAD 照合 / `ERGO_PACKAGE_SOURCE_DIR_<NAME>` / `ERGO_PACKAGES_SIBLING_ROOT` (`<root>/<Repo>` だけ) / `find_package` (宣言があるものだけ、STRICT では使わない) / `ERGO_PACKAGES_FETCH=ON` のときだけ取得 (`_ergo_fc_declare` 再利用、`GIT_SHALLOW`、`ERGO_DEPENDENCY_CACHE_DIR` 共有、HEAD == pin 検証) / 構成エラー) | `cmake/packages/ErgoPackageResolve.cmake` |
| 2 | git の読み取り専用照会 (HEAD・祖先・dirty・expect_tag・shallow) | `cmake/packages/ErgoPackageGit.cmake` |
| 3 | 結線の部品 (cache 既定値は FORCE しない・feature 写像・`add_subdirectory(... EXCLUDE_FROM_ALL)`・targets / provides / contract 検証・exports) と 1 パッケージ 1 ファイルの結線 | `cmake/packages/ErgoPackageWiring.cmake`、`cmake/packages/{pictor,tela}.cmake` |
| 4 | 検査 PKG-03/04/05/07/08/09/10/16、drift 判定 none / ahead / behind / diverged / unknown | `cmake/packages/ErgoPackageRulesAcquire.cmake` (`ErgoPackageRules.cmake` から include) |
| 5 | 入口: M1 / M2、`ERGO_PACKAGE_PROVIDED_<NAME>` (後検証)、`ERGO_PACKAGE_REV_<NAME>` (照合に使う)、`ERGO_PACKAGES_STRICT`、`ERGO_PACKAGE_MANIFEST_EXTRA` (同名は構成エラー)、include 時と末尾の 2 段で FATAL を全件列挙 | `cmake/packages/ErgoPackages.cmake`、`ErgoPackageManifest.cmake` (`ergo_pkg_manifest_merge`、`wiring` / `exports` / feature `cache` の読み取り) |
| 6 | resolved.json に `route` / `mode` / `head` / `drift` / `dirty` / `exports`、`fetch` / `manifest_extra`、状態 `hub-managed` | `cmake/packages/ErgoPackageReport.cmake` |
| 7 | `ci-packages` プリセット (`ERGO_PACKAGES=pictor;tela` + FETCH + STRICT) | `CMakePresets.json` |
| 8 | テスト: 取り込み段の規則 (`cmake -P`) と偽パッケージ 3 個の構成テスト (git 初期化はテスト内) を ctest に登録。M0 不変テストを M1 の期待値に更新 | `tests/packages/test_acquire.cmake`、`test_hub_routes.cmake`、`fixtures/{alpha,beta,beta_install,gamma,hub,hub_host}/`、`test_m0_invariance.cmake`、`CMakeLists.txt` |
| 9 | 仕様: M1 / M2 の手順、スキーマの該当箇所、統合設計の配置表と実装状況 | `spec/setup/packages.md`、`spec/data/ergo-packages.md`、`spec/feature/package-hub.md`、`README.md`、`module_list.md` |

## 受け入れ条件 (契約書式)

- C-1 ergo_pkg_resolve(prefix, name, pin, features): 6 経路を host → (provided) → source → sibling → installed → fetch → PKG-03 の順に試し、最初に当たった経路で取り込み、誤った `ERGO_PACKAGE_SOURCE_DIR_<NAME>` は次へ進まず PKG-03、兄弟は `<root>/<Repo>` だけを読み `.worktrees` を見ず、`installed` は STRICT で使わず、取得は `ERGO_PACKAGES_FETCH=ON` のときだけ — `test_packages_hub_routes` (host_none / provided_late / source_none / source_missing / sibling / sibling_worktrees / installed / installed_strict / fetch_off / fetch_ok)
- C-2 ergo_pkg_drift_classify / ergo_pkg_rule_drift(…): none / ahead / behind / diverged / unknown を判定し、PKG-05 / PKG-09 は警告で STRICT は error、installed は記録、fetch は PKG-04 に任せる — `test_packages_acquire` と `test_packages_hub_routes` (host_ahead / source_behind / source_diverged / source_unknown / *_strict)
- C-3 ergo_pkg_rule_fetched_head / ergo_pkg_rule_dirty / ergo_pkg_rule_expect_tag / ergo_pkg_rule_layout / ergo_pkg_rule_submodule(…): PKG-04 / 07 / 08 / 10 / 16 の真偽両方と STRICT 昇格 — `test_packages_acquire` と `test_packages_hub_routes` (fetch_mismatch / source_dirty / expect_tag / host_contract_missing / provided_missing / tool_uninit)
- C-4 ergo_pkg_wire_default(): cache 既定値はホスト未設定のときだけ入り FORCE せず (ホスト値が勝ち PKG-15)、選んだ機能の `cache` は結線のスコープ内だけに流れ、`exports` が Ergo のディレクトリに渡る — `test_packages_hub_routes` (source_none / source_host_cache / emscripten_web の leak 判定)
- C-5 ergo_pkg_manifest_merge(prefix, extra, findings): 追加マニフェストの新しい名前は取り込み、同名は PKG-15 の error で M0 でも構成を止める — `test_packages_acquire` と `test_packages_hub_routes` (extra_ok / extra_clash / extra_clash_m0)
- C-6 ErgoPackages.cmake (Emscripten 模擬): `EMSCRIPTEN=ON` で gpu (vulkan 相当) の明示選択が PKG-13、web が既定で ON になり web ターゲットだけを要求する — `test_packages_hub_routes` (emscripten_web / emscripten_gpu)
- C-7 _ergo_packages_evaluate(phase): error は include 時・末尾のどちらでも全件を並べてから止め、非 STRICT で通した findings は resolved.json に残る — `test_packages_hub_routes` (all_listed / source_dirty)
- C-8 ErgoPackages.cmake (M0): `ERGO_PACKAGES` 未設定で既存構成のターゲット集合と compile definitions が変わらない (S1 の C-5 を維持) — `test_packages_m0_invariance`、実ツリーで `compare_m0_build.cmake`

Augur の契約 (`augur.contracts.json` と述語モジュール) は用意できない。Augur の `contract-wrap` は TypeScript の関数を
構文解析して包む仕組みで、CMake の関数は対象にならない。上の受入条件は ctest のテスト名で裏づける。

## 再利用探索の採否

| 候補 | 採否 | 理由 |
|---|---|---|
| `cmake/ErgoDependencies.cmake` の `ergo_declare_dependency` / `_ergo_fc_declare` | 採用 | 取得経路はこれを通す (`GIT_SHALLOW`、`ERGO_DEPENDENCY_CACHE_DIR` 共有、`ERGO_SOURCE_DIR_<X>` / `FETCHCONTENT_SOURCE_DIR_<X>` の上書きもそのまま効く)。依存名は `ergo_pkg_<name>` |
| `ergo_populate_dependency` (`FetchContent_Populate`) | 不採用 | CMake 3.30 以降で単一引数の `FetchContent_Populate` は非推奨。`SOURCE_SUBDIR` を存在しない所に向けた `FetchContent_MakeAvailable` で取得だけを行う |
| `ergo_verify_targets` | 不採用 | 見つからないと即 FATAL で、全件列挙 (PKG-10 を集めてから止める) にならない |
| `cmake/ErgoRenderBackend.cmake` | 変更せず | `PICTOR_HAS_VULKAN` の判定はそのまま。ハブは `contract` で同じ定義を検証するだけ |
| S1 の `ErgoPackageRules.cmake` | 拡張せず別ファイル | 取り込み段の規則を `ErgoPackageRulesAcquire.cmake` に分け、`ErgoPackageRules.cmake` の末尾で include (呼び出し側は 1 ファイル) |
| S1 の `_ergo_packages_finalize` | 2 段の評価に作り替え | 取り込みはモジュール節より前でないと `if(TARGET pictor)` が効かないため include 時に解決し、モジュール依存の検査は末尾に残す |

## 判断したこと (前提未確定を含む)

- 経路は**パッケージごと**に決まり、resolved.json の全体 `mode` は `M1` / `M2` / `M1+M2`。S1 の `selected` は廃止
- `ERGO_PACKAGE_PROVIDED_<NAME>`: ターゲットが既にあれば `provided` 経路で即検証、無ければハブは取り込まずディレクトリ末尾で検証
- `ERGO_PACKAGE_REV_<NAME>` は記録 (PKG-06) に加え、照合と取得もこの SHA で行う (検証用の一時上書きという設計の意図)
- 宣言ターゲットの必須判定: `cmake.targets` のうち機能の `provides` に載るもの (Pictor の `pictor` は `vulkan`) はその機能を選んだときだけ必須。
  現 pin の Pictor は Emscripten で `pictor` を作らないため (`if(EMSCRIPTEN) ... return()`)、マニフェストを変えずに Emscripten を通すための規則
- Pictor の `vulkan` には切り替えオプションが無いので、`pictor.cmake` が Pictor の配下だけで `CMAKE_DISABLE_FIND_PACKAGE_Vulkan` を立てる
- 機能の `cache` は結線スコープ内の通常変数で流す (`option()` は CMP0077 NEW で従う)。キャッシュに残らないため、機能を外した再構成で古い値が残らない
- dirty は追跡ファイルの差分だけを見る (`--untracked-files=no`。checkout 内のビルドディレクトリを dirty にしない)
- shallow な checkout (取得経路) では祖先関係を判定しない (drift は HEAD == pin だけ、PKG-08 は判定しない)
- 追加マニフェストの同名衝突は M0 でも止める (ホストが明示的に設定した変数のため。何も設定しないホストの M0 保証は保たれる)
- tool の submodule 未初期化 (PKG-16) は M0 でも警告として表示する (構成は止めない)

## 変更した境界と復旧方法

- 既存ホストへの影響の境界: `ERGO_PACKAGES` 未設定 (M0) ではターゲット・compile definitions・リンクを変えない。
  増えるのはキャッシュ項目 `ERGO_PACKAGES_FETCH` / `ERGO_PACKAGES_SIBLING_ROOT` / `ERGO_PACKAGE_MANIFEST_EXTRA`、
  resolved.json の項目、tool submodule 未初期化の警告行 (PKG-16)、ctest のテスト 2 件 (ビルドターゲットではない)
- `ERGO_PACKAGES` を設定していたホスト (S1 では検査だけ): S2 では選んだパッケージを実際に解決する。ホストが先にターゲットを作っていれば
  M1 (照合のみ) なので取り込みは起きない。作っていなければ以前は WARNING で通っていたものが PKG-03 で止まる
- ネットワークに出るのは `ERGO_PACKAGES_FETCH=ON` のときだけ
- 復旧方法: ホスト側は `ERGO_PACKAGES` を外せば M0 に戻る (データ移行なし)。Ergo 側は本 PR の squash commit を revert する

## 検証の記録

| 項目 | 結果 |
|---|---|
| `cmake -P` 規則テスト 6 本 (manifest / rules / cycles / features / report / acquire、CMake 4.1.2) | 全 PASS |
| `test_hub_routes.cmake` (Visual Studio 17 2022 生成器、構成 32 件・18 群) | 全 PASS |
| `test_m0_invariance.cmake` (7 ケース、選択時の期待値を M1 に更新) | 全 PASS |
| `compare_m0_build.cmake` (基準 = 分岐元 5dfb8d6、新 = 本ブランチ、VS 2022 x64、既定オプション) | 464 行 (ターゲット × 構成、compile definitions 込み) 一致 |
| `ctest -C Release -R test_packages` (上の比較で構成した実ツリー) | 8/8 PASS |
| C++ のビルド | 未実施 (C++ の変更なし) |
| GCC での構成 | 未実施 (手元に GCC 環境なし。CMake スクリプトのみ) |
| Pictor 実ソースを `ERGO_PACKAGE_SOURCE_DIR_PICTOR` で取り込み、`ergo_render` の実経路判定が今と同じか | 未確認 (実ホスト・実ソースでの構成は人の許可に従う。desktop / Android / iOS / Emscripten とも) |
| `ci-packages` プリセットでの実取得 | 未確認 (ネットワーク取得は行っていない。取得経路はテスト内の file:// リポジトリで確認) |
