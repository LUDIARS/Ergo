# パッケージマネージャ W6 — P4 ホスト状態図 (Ergo 側)

- 日付: 2026-10-15
- タスク参照: `actio:3b0bb096-1da7-4cbb-9dfd-e9af9a842e8c` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-manager.md`](../feature/package-manager.md) §5.1 (P4 ホスト状態図) / §7 の W6 行と受入条件 (Ergo 側) / §8 / §9 (W-6 は推奨どおり)
- 前提: W3 / W4 ([`2026-10-15-package-manager-w3.md`](2026-10-15-package-manager-w3.md)、[`2026-10-15-package-manager-w4.md`](2026-10-15-package-manager-w4.md))、S4 の usage 図 (main マージ済み)
- 形式: [`spec/data/ergo-packages-status.md`](../data/ergo-packages-status.md) (新規)、[`spec/data/ergo-watch-report.md`](../data/ergo-watch-report.md) (読むだけ)
- ブランチ: `feat/package-manager-w6`
- 範囲外: Ludellus-Native の L1〜L3 (§5.2、Ludellus-Native の PR)。Ludellus-Native のファイルは変更していない。見本のホストは `Example`

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | resolved.json の `host.id` を読む | `package_model.h` (`host_id`)、`resolved_reader.cpp` |
| 2 | P4 の入力の値型と読み手 (報告・状態)。欠けた・壊れた入力は `has_*` が false で notes に理由 | `include/ergo/package_model/host_state.h` / `host_state_reader.h`、`src/package_model/watch_report_reader.cpp` / `host_status_reader.cpp` / `host_state_loader.cpp` |
| 3 | テスト結果の集計 (JUnit → LastTest.log) と状態ファイルの書き手 | `include/ergo/package_manager/test_results.h` / `host_status.h`、`src/package_manager/test_results.cpp` / `host_status.cpp` |
| 4 | CLI `ergo_pkg status --build <dir> [--junit <xml>] [--out <file>]` | `tools/package_cli/cmd_status.cpp` / `status_inputs.*`、`cli_args.*`、`commands.h`、`main.cpp` |
| 5 | P4 の組み立て (グループ 6、実線と破線、由来ごとの畳み込み、札) | `include/ergo/package_viz/host_view.h`、`src/package_viz/host_view.cpp` / `host_labels.*`、`groups.*` (`host_groups`)、`view_graph.h` (`View::Host`)、`fit.cpp` (`make_view` は P4 を拒む) |
| 6 | CLI `ergo_package_viz --view host [--host DIR] [--report FILE] [--status FILE]`、`ergo_package_graphs` に `host.tela` | `tools/package_viz/host_command.*` / `cli_options.*` / `main.cpp`、`CMakeLists.txt` |
| 7 | テスト (JSON / XML 見本、ネットワーク無し) | `tests/package_model/test_host_state.cpp`、`tests/package_manager/test_host_status.cpp`、`tests/package_viz/test_host_view.cpp`、`tests/packages/test_package_host_view.cmake`、見本 `tests/package_model/fixtures/host_state/`・`tests/package_manager/fixtures/status/` |
| 8 | 文書 | `spec/data/ergo-packages-status.md` (新規)、`spec/module/package_viz.md` / `package_model.md` / `package_manager.md`、`spec/feature/package-manager.md` (実装状況)、`spec/domains/build-packages.domain.json` (specRefs)、`module_list.md` / `.yaml`、`README.md` |

新しいモジュールと新しいトップレベルは無い (全て既存の `ergo_package_model` / `ergo_package_viz` / `ergo_package_manager` と
`tools/package_viz/`・`tools/package_cli/` の中)。`spec/domains` の membership と `.anatomia/layers.json` は既存のパターンが
新しいファイルを含むので変えない (`build-packages` の specRefs に新しい形式の文書だけ足した)。

## 受け入れ条件 (契約書式)

- C-1 make_host_view(state): resolved + 報告 + JUnit 由来の状態の見本から P4 を作り `validate_tela_graph` を通る — `test_package_viz` (PackageVizHostView.FixtureGraphPassesTelaValidation、FixtureNodesEdgesAndLabels)、`test_package_manager` (PackageManagerHostStatus.WritesTheP4FixtureBytes: 状態の見本が JUnit 見本から作ったバイト列と一致)、`test_package_host_view`
- C-2 build_host_view(state): 審査中・作業中 20 件で由来ごとに畳まれ (4 ノード) 上限内、上限ちょうどは畳まない — `test_package_viz` (PackageVizHostView.TwentyReviewsFoldByOrigin)
- C-3 load_host_state / build_host_view: 入力が欠けても図が出る (札 `—`)。壊れた・別 schema の入力は無いのと同じ — `test_package_model` (PackageModelHostState.MissingInputsAreNotErrors、RejectsMalformedAndOtherSchemas)、`test_package_viz` (PackageVizHostView.MissingInputsStillDraw)、`test_package_host_view`
- C-4 make_host_view / host_status_text: 決定的 (同じ入力で同じ `.tela` / status.json のバイト列) — `test_package_viz` (PackageVizHostView.Deterministic)、`test_package_manager` (WritesTheP4FixtureBytes)、`test_package_host_view`
- C-5 build_usage_view: S4 の usage 図のテストが無変更で通る — `tests/package_viz/test_usage_view.cpp` は変更していない (`test_package_viz` に同居)
- C-6 parse_junit / parse_last_test_log / `ergo_pkg status`: `--junit` を優先、無ければ LastTest.log、無ければ unknown。状態にパス・テスト名を載せない — `test_package_manager` (CountsJunit、CountsLastTestLog、UnknownTestsCarryNoCounts、StatusHasNoLeaks)、`test_package_host_view`

Augur の契約 (`augur.contracts.json` と述語モジュール) は用意できない。Augur の `contract-wrap` は TypeScript の関数を
包む仕組みで、C++ / CMake は対象にならない (W1〜W4 と同じ)。上の受入条件は ctest のテスト名で裏づける。

## 判断 (§5.1・§9 と実装を見て決めたこと)

| 論点 | 決めたこと | 根拠 |
|---|---|---|
| 報告と状態の読み手の置き場所 | `ergo_package_model` (`host_state_reader.h`) | §7 のモジュールの向きは `ergo_package_viz → ergo_package_model` だけで、viz は package_watch / package_manager を import できない。共有するのは形式だけ (Unity の読み手と同じ考え方) |
| 状態の書き手の置き場所 | `ergo_package_manager` (純粋関数) + CLI (ファイル・時計・git) | `ergo_pkg` の他のコマンドと同じ分け方。ライブラリはファイルも時計も触らない |
| 依存 → 検査の「点線」 | 破線にし、検査のグループの色で見分ける | Tela の辺は実線と破線しか持たない (`tela_graph::Edge::dashed`)。形式を変えずに描ける範囲にした |
| 畳み込みの上限 | 審査中・作業中が 12 件を超えたら由来 (Pf / AI / 人 / 不明) ごとに 1 ノード | 受入の「20 件で畳まれ上限内」を満たし、少ないときは 1 件ずつ見える。番号は札に 6 件まで |
| 依存の集め方 | resolved.json の link のうち `absent` でないもの + 報告の依存 (Ergo 自身を除く) | どちらか片方しか無くても描ける (入力が欠けても図が出る)。tool / observe は P1 / P3 の領分 |
| 検査として描くもの | findings の error / warning (record は描かない) | §5.1 の「落ちた PKG-xx」。札は ID と対象だけ (message はパスを含み得る) |
| テストの「未実行」 | 状態が無ければ `—`、状態の `tests.source` が unknown なら `未実行` | 「入力が無い」と「入力はあるがテストが走っていない」を分ける |
| `ergo_package_graphs` と `ergo_pkg status` | graphs は status を起こさない (読むだけ) | `ergo_pkg` は別のオプションの別ターゲットで、テスト結果はテストを走らせた人の手元にしか無い。手順は `spec/module/package_viz.md` に書いた |
| 改行 | 状態の見本との比較は CR を除いてから | `core.autocrlf=true` の checkout で見本が CRLF になる |

## 再利用探索の採否

| 候補 | 採否 | 理由 |
|---|---|---|
| `fit_view` (簡約・焦点・分割・検査) | 採用 | P1〜P3 と同じ手順で上限に収める。`--focus` も `NodeInfo.package` で効く |
| `ergo_graph_layout` / `ergo_tela_graph` の writer・検査 | 採用 | §5.1「配置は `ergo_graph_layout` を再利用」 |
| `package_model` の `json_access` / `read_resolved_text` | 採用 | resolved.json の読み取りをそのまま使い、報告・状態の読み手も同じ部品で書いた |
| `ergo_package_watch` の報告の型・読み手 | 不採用 | viz → watch の import は §7 の向きに反する。報告の書き手はあるが読み手は無く、図に要るキーだけを model 側で読む |
| `ergo_package_manager` の漏れ検査 | 採用 | `ergo_pkg status` が書く前に当てる (§8) |
| `tools/package_cli/process.h` の `git_head` | 採用 | ホストの HEAD |
| JUnit を読む既存の部品 | 無し | Ergo にも third_party にも XML の読み手が無い。ctest の出力の形だけを見る小さな走査にした (新規の third-party は入れない) |

## 検証

| 何を | 結果 |
|---|---|
| MSVC (VS 2022, x64, Debug) で構成し `test_package_model` / `test_package_viz` / `test_package_manager` / `test_package_watch` / `ergo_package_viz_cli` / `ergo_pkg_cli` をビルド | 成功 (警告は既存の `json_min.h` の C4819 のみ) |
| GCC でのビルド | 未実施 |
| テストの実行 (ctest、`cmake -P` のテストを含む) | 未実施 (委託の指示で、明示の指示が無い限りテストを実行しない。Revisor の審査に任せる) |
| `tela_view` での見え方 | 未確認 (画面確認はしていない) |
| 秘匿語の辞書照合・絶対パス | 提出前に差分で 0 件を確認 |

## 復旧

- 図だけ止める: `ergo_package_graphs` の `--view host` の 1 行を外す (他の図と独立)。
- 状態ファイルはビルドディレクトリの中にしか無い。消せば P4 のテストの札が `—` になるだけ。

## 残り (W6 の Ergo 側の外)

- Ludellus-Native の L1〜L3 (§5.2、Ludellus-Native の PR)。L1 で `ergo_pkg init --code Ln`、L2 / L3 の確認に P4 を使う
- `ergo_package_graphs` の前に `ergo_pkg status` を起こすかどうか (今は手で打つ)
- ビルドの成否・CI の結果を状態に足す (§9 W-6、S7)
- `tela_view` での P4 の見え方の確認

## 提出記録

- 提出: Revisor local PR (ブランチ `feat/package-manager-w6`、base `main`)。GitHub PR は作らない
