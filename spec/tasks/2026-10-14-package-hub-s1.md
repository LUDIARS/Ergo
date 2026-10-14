# パッケージ・ハブ S1 — マニフェストと規則 (記録のみ)

- 日付: 2026-10-14
- タスク参照: `actio:d0abfb44-da35-4684-829c-e6b4394eb787` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-hub.md`](../feature/package-hub.md) §2 / §4 / §6 / §7 の S1 行
- ブランチ: `feat/package-hub-s1`

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | マニフェスト (schema 1)。Pictor / Tela の link 行 (rev は各リポのローカル main の 40 桁 SHA)、Lapilli の tool 行、modules 節、observe 節。未対応の描画方式は reserved | `ergo-packages.json` |
| 2 | マニフェストの読み取り (JSON → リスト変数) | `cmake/packages/ErgoPackageManifest.cmake` |
| 3 | 純粋な検査規則 PKG-01 / 02 / 06 / 11 / 12 / 13 / 14 / 15 | `cmake/packages/ErgoPackageRules.cmake` |
| 4 | resolved.json の書き出し (`file(CONFIGURE)`、決定的) と File API query (client-ergo codemodel v2) | `cmake/packages/ErgoPackageReport.cmake` |
| 5 | 入口。`ERGO_PACKAGES` 未設定 = M0 (取り込まず host-unmanaged を記録)。取り込み経路は S2 | `cmake/packages/ErgoPackages.cmake`、`CMakeLists.txt` の include 1 行 |
| 6 | `cmake_minimum_required` 3.16 → 3.20 | `CMakeLists.txt`、`spec/setup/*.md` の版表記 |
| 7 | スキーマ説明とホスト向け手順 | `spec/data/ergo-packages.md`、`spec/setup/packages.md` |
| 8 | 規則テスト (`cmake -P`) を ctest に登録 | `tests/packages/test_{manifest,rules,cycles,features,report}.cmake` |
| 9 | C-5 の確認手段 (偽ホストの構成テスト + 実ツリーの比較スクリプト) | `tests/packages/test_m0_invariance.cmake`、`fixtures/m0_host/`、`compare_m0_build.cmake` |
| 10 | ドメイン・一覧・README | `spec/domains/foundation.domain.json`、`spec/domains/README.md`、`module_list.md` / `.yaml`、`README.md` |

## 受け入れ条件 (契約書式)

- C-1 ergo_pkg_rule_unknown_names / ergo_pkg_rule_rev_format / ergo_pkg_rule_rev_override / ergo_pkg_rule_requires / ergo_pkg_rule_cmake_minimum / ergo_pkg_rule_module_needs / ergo_pkg_rule_collisions(…): PKG-01 / 02 / 06 / 11 / 14 / 15 がそれぞれ真 (finding あり) と偽 (finding なし) の両方で期待どおりに判定し、STRICT で warning が error に上がり、M0 の PKG-14 は record に留まる — `test_packages_rules`
- C-2 ergo_pkg_rule_cycles(NODES, EDGES): 自己ループ・2 点・3 点の循環を検出して実際の循環経路を示し、非循環 (鎖・菱形・孤立点) では何も出さず、辺の順序に結果が依存しない — `test_packages_cycles`
- C-3 ergo_pkg_features_select / ergo_pkg_rule_feature_combination(…): 既定 (auto / on / off) が platform に従い、`+f` / `f` / `-f` の足し引きが順に効き、不明名・不正トークン・予約機能・platform 不一致・conflicts・描画機能 0 が PKG-13、機能 requires 未充足が PKG-11 になる — `test_packages_features`
- C-4 ergo_pkg_report_render / ergo_pkg_report_write(…): 同じ事実から同じバイト列を作り、入力の並び順に依存せず、時刻を含まず、同じ内容なら書き直さず、出力が JSON として読め、File API query が `client-ergo` に codemodel v2 で置かれる — `test_packages_report`
- C-5 ErgoPackages.cmake (M0): `ERGO_PACKAGES` 未設定で既存構成のターゲット集合と compile definitions が変わらない — 偽ホストで `test_packages_m0_invariance`、実ツリーで `compare_m0_build.cmake`
- C-6 ergo_pkg_manifest_load(prefix, ergo-packages.json): 実マニフェストが schema 1 で読め、link の pin がすべて 40 桁 SHA、宣言グラフに循環が無く、ターゲットの重複が無く、Pictor の既定機能が宣言した全 platform で検査を通る — `test_packages_manifest`

## 再利用探索の採否

| 候補 | 採否 | 理由 |
|---|---|---|
| `cmake/ErgoDependencies.cmake` (`_ergo_fc_declare` / `ergo_verify_targets`) | 採用見送り (S2 で使う) | S1 は取得しないため呼ぶ所が無い。流儀 (エンジンは具体名を持たない、無言フォールバック禁止、`include_guard(GLOBAL)`) は合わせた |
| `cmake/ErgoRenderBackend.cmake` | 変更せず値だけ読む | `ERGO_RENDER_BACKEND_ENABLED` / `_REASON` をマニフェストの `real_path_var` / `reason_var` 経由で resolved.json と PKG-14 に写す |
| `ergo_common::jsonm` | 不使用 | 構成時の読み取りは CMake の `string(JSON)` で足りる (C++ 側の読み取りは S3) |
| 統合設計 §6 の 3 ファイル構成 | 4 ファイルに分割 | マニフェストの読み取りを規則・記録・入口のどれにも持たせないため `ErgoPackageManifest.cmake` を分けた (SRP) |

## 変更した境界と復旧方法

- 既存ホストへの影響の境界: `ERGO_PACKAGES` 未設定 (M0) ではターゲット・compile definitions・リンクを一切変えない。
  増えるのはビルドディレクトリの `ergo-packages.resolved.json`、`.cmake/api/v1/query/client-ergo/query.json`、
  構成ログの要約 1 行、キャッシュ項目 `ERGO_PACKAGES` / `ERGO_PACKAGES_STRICT`、ctest のテスト 6 件 (ビルドターゲットではない)
- 唯一の互換性の変化: `cmake_minimum_required` 3.20。CMake 3.16〜3.19 のホストは構成できなくなる。
  3.20 で NEW になる方針 (CMP0097〜CMP0120) で生成結果が変わらないことは実ツリーの比較で確認 (VS 2022)
- 復旧方法: ハブだけを外すなら `CMakeLists.txt` の `include(.../cmake/packages/ErgoPackages.cmake)` 1 行と
  tests/packages の登録節を消す。全体を戻すなら本 PR の squash commit を revert する (データ移行は無い)

## 検証の記録

| 項目 | 結果 |
|---|---|
| `cmake -P` 規則テスト 5 本 (CMake 4.1.2) | 全 PASS |
| `test_packages_m0_invariance` (Visual Studio 17 2022 生成器、7 ケース) | 全 PASS |
| `ctest -C Release -R test_packages` (実ツリーを VS 2022 x64 で構成した build) | 6/6 PASS |
| `compare_m0_build.cmake` (基準 = 分岐元 commit、新 = 本ブランチ、VS 2022 x64、既定オプション) | 464 行 (ターゲット × 構成、compile definitions 込み) 一致 |
| C++ のビルド | 未実施 (C++ の変更なし。CMake の構成と cmake -P のみ) |
| GCC での構成 | 未実施 (手元に GCC 環境なし。CMake スクリプトのみでコンパイラ非依存) |
| 既存ホスト 1 つを M0 で構成して結果不変 | 未確認 (実ホストでの構成は人の許可に従う) |
| CMake 3.20 ちょうどでの構成 | 未確認 (手元は 4.1.2) |
