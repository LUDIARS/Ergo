# パッケージ・ハブ S3 — 可視化 P1 版一覧 / P2 リンク図

- 日付: 2026-10-14
- タスク参照: `actio:d0abfb44-da35-4684-829c-e6b4394eb787` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-hub.md`](../feature/package-hub.md) §5 収集と可視化 / §6 配置 / §7 の S3 行
- 前段: [`2026-10-14-package-hub-s1.md`](2026-10-14-package-hub-s1.md)、[`2026-10-14-package-hub-s2.md`](2026-10-14-package-hub-s2.md) (main 取込済み)
- ブランチ: `feat/package-hub-s3`

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | 汎用の層別配置 (最長経路の列、重心法 2 往復 + ID 順、通り道の枠、縦横の折れ線、長さ 0 なし、札の行数の高さ、±100000、決定的) | `include/ergo/graph_layout/` · `src/graph_layout/` · `tests/graph_layout/`、`spec/module/graph_layout.md` |
| 2 | jsonm で File API が読めるかの確認と、足りないサロゲートペア復号の読み取り専用拡張 | `src/common/json_min.cpp` |
| 3 | resolved.json リーダ、File API codemodel v2 リーダ (version 検査・最新 index・生成器ターゲット除外)、帰属 (最長一致・ビルドツリー内の第三者)、既定の除外、層 (`.anatomia/layers.json`)、PKG-17、版の札 | `include/ergo/package_model/` · `src/package_model/` · `tests/package_model/`、`tests/package_model/fixtures/`、`spec/module/package_model.md` |
| 4 | P1 / P2 の抽象グラフ、推移簡約、畳み込みと焦点、分割と索引、上限に収める手順と検査 | `include/ergo/package_viz/` · `src/package_viz/` · `tests/package_viz/`、`spec/module/package_viz.md` |
| 5 | CLI `ergo_package_viz` (`--view` / `--focus` / `--expand` / `--check` / 版の照会) と `ergo_package_graphs` | `tools/package_viz/`、`CMakeLists.txt` |
| 6 | resolved.json に P1 が要る `group` / `requires` を足す (マニフェストに `group`) | `ergo-packages.json`、`cmake/packages/{ErgoPackageManifest,ErgoPackages,ErgoPackageReport}.cmake`、`tests/packages/test_report.cmake` |
| 7 | 文書: モジュール一覧・README・ドメイン・層・スキーマ・手順・統合設計の実装状況 | `module_list.md` / `.yaml`、`README.md`、`spec/domains/observability.domain.json`、`.anatomia/layers.json`、`spec/data/ergo-packages.md`、`spec/setup/packages.md`、`spec/feature/package-hub.md` |

## 受け入れ条件 (契約書式)

- C-1 layout_layered(input, options): 左 = 使う側・最長経路の列・同順位は ID 順で、辺は縦横の折れ線で長さ 0 の区間を持たず途中の列のノードを横切らず、同じ入力から同じバイト列、±100000 内 — `test_graph_layout`
- C-2 read_resolved_text / read_codemodel(build_dir): schema 1 と codemodel major 2 以外を明示エラーにし、最新 index の client-ergo 応答を読み、生成器ターゲットと UTILITY を読まない — `test_package_model` (ResolvedReader* / Codemodel*)
- C-3 attribute_targets(model): 定義ディレクトリの最長一致で帰属し、ビルドツリー内の取得物は第三者、テスト・ダミー・ベンチ・ergo_gtest_main を既定で除外、Ergo のターゲットに層を割り当てる — `test_package_model` (Attribution / DefaultExclusions / Layers*)
- C-4 find_undeclared_links(model): 宣言 (needs / optional / requires) に無い Ergo・link → link の実リンクを PKG-17 として返し、CLI `--check` は 1 件以上で終了コード 1 — `test_package_model` (UndeclaredLinks* / LinkPackageRequires*)
- C-5 build_packages_view / build_targets_view(model): P1 は札に版 (出所付き)・SHA7・機能・印、辺は requires と畳んだ実リンク (実線) と移植・語彙共有 (破線)、P2 は 5 層 + 外部 + 第三者 + 循環 — `test_package_viz` (PackagesView* / TargetsView* / Cycles*)
- C-6 fit_view(view, model, options): 既定除外 → 推移簡約 → 焦点と畳み込み → 分割の順に収まった所で止め、上限ちょうどは 1 枚、超過は `out.N.tela` + `out.index.tela`、どの枚も validate_tela_graph を通る (通らない 1 枚は書かない) — `test_package_viz` (Transitive* / Collapse* / Focus* / Limit* / OverLimit* / TooManyEdges* / Split* / Deterministic*)
- C-7 tela::load_graph(file): Tela がある構成では書いた P1 / P2 を Tela 本体が受理しプロセス内変換と同じ図になる — `test_package_viz_tela` (`Tela::Graph` がある構成だけ)
- C-8 ErgoPackageReport.cmake: resolved.json の `group` / `requires` が整列済みで出て、M0 の不変 (S1 の C-5) を保つ — `test_packages_report`、`test_packages_m0_invariance`

Augur の契約 (`augur.contracts.json` と述語モジュール) は用意できない。Augur の `contract-wrap` は TypeScript の関数を
構文解析して包む仕組みで、C++ / CMake の関数は対象にならない。上の受入条件は ctest のテスト名で裏づける。

## 再利用探索の採否

| 候補 | 採否 | 理由 |
|---|---|---|
| `ergo_render_viz` の配置 (`layout_pipeline`) | 不採用 (参照のみ) | 描画パイプライン専用の行配置で、任意の有向グラフの層別配置ではない。統合設計 U7 で寄せないと決めている。長さ 0 の区間を作らない `push_point` の考え方だけ踏襲 |
| `ergo_tela_graph` (`Graph` / `validate_tela_graph` / `save_tela_graph`) | 採用 | 出口と検査をそのまま使う。上限定数 (`kTelaMax*`) も共有 |
| `ergo_common::jsonm` | 採用 + 拡張 | resolved.json / File API / layers.json を読める。サロゲートペアの復号だけ足した (読み取り専用、新規 third-party なし) |
| CMake の `string(JSON)` で File API を読む | 不採用 | File API の返答は generate の後にしかできず、構成時には読めない。C++ 側で読む |
| `tests/tela_graph/test_tela_graph_adapter.cpp` の契約テストの形 | 採用 | Tela がある構成だけの受理テストを同じ形で足した |
| Anatomia の層判定 | 不採用 (宣言だけ共有) | 実行時に Anatomia を呼ばず、`.anatomia/layers.json` の glob を同じ規則 (glob 順の先勝ち) で読む |

## 検証

| 何を | 結果 |
|---|---|
| MSVC (VS 2022, x64, Debug) で `test_graph_layout` / `test_package_model` / `test_package_viz` / `ergo_package_viz_cli` をビルド | 成功 (警告は既存の `json_min.h` の C4819 のみ) |
| テストの実行 (ctest) | 未実施 (委託の指示で、明示の指示が無い限りテストを実行しない) |
| GCC でのビルド | 未実施。designated initializer を使わず、`std::isdigit` 等を関数ポインタで渡さず、`u8string` の型差を吸収した |
| `test_package_viz_tela` (Tela がある構成) | 未実施 (この構成に Tela が無い) |
| `ergo_package_graphs` の実行・`tela_view` での画面確認・実ホストでの図 | 未実施 (画面確認と実ホストの構成はしない指示) |
| 秘匿語の辞書照合 | 提出前にファイルとコミットで 0 件を確認 |

## 復旧

- 図の機能を止める: `-DERGO_BUILD_PACKAGE_VIZ=OFF` (CLI と `ergo_package_graphs` も消える)。モデルと配置も不要なら
  `-DERGO_BUILD_PACKAGE_MODEL=OFF -DERGO_BUILD_GRAPH_LAYOUT=OFF`。既存モジュールはどれも新モジュールに依存しない。
- resolved.json の `group` / `requires` は読み手が増えるだけの追加フィールド。戻すときはこの PR を revert する
  (M0 のビルド結果には影響しない)。
