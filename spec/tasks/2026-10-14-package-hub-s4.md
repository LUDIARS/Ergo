# パッケージ・ハブ S4 — 観測 P3 使用関係図

- 日付: 2026-10-14
- タスク参照: `actio:d0abfb44-da35-4684-829c-e6b4394eb787` (本文は Actio が正本。ここには転記しない)
- 設計の正本: [`spec/feature/package-hub.md`](../feature/package-hub.md) §2 observe / §5 収集と可視化 (P3・scan) / §7 の S4 行 / N5・N8
- 前段: [`2026-10-14-package-hub-s1.md`](2026-10-14-package-hub-s1.md)、[`-s2.md`](2026-10-14-package-hub-s2.md)、[`-s3.md`](2026-10-14-package-hub-s3.md) (main 取込済み)
- ブランチ: `feat/package-hub-s4`

## 分解

| # | 作業 | 成果物 |
|---|---|---|
| 1 | 読み取り部品: JSON の行索引、`.gitmodules`、git index の gitlink (版 2 / 3)、`package.json` の依存の選別、マニフェストの observe / ported_from / tool | `include/ergo/package_scan/` · `src/package_scan/{json_lines,gitmodules_reader,git_index_reader,package_json_reader,manifest_reader}.cpp` |
| 2 | 範囲 (Scope) と読み取り元、依存先の帰属、`scan_usage` (根拠付きの辺・未観測・読んだファイルの記録) | `src/package_scan/{scope,source,resolver,scan,usage_model}.cpp`、`spec/module/package_scan.md` |
| 3 | P3 usage の抽象グラフ (6 グループ・実線/破線・根拠の札) と `make_usage_view` (P1/P2 と同じ fit 手順) | `include/ergo/package_viz/usage_view.h`、`src/package_viz/{usage_view,groups,fit}.cpp`、`spec/module/package_viz.md` |
| 4 | CLI `--view usage` / `--scan` / `--root` / `--manifest`、`ergo_package_graphs` に usage | `tools/package_viz/{cli_options,usage_command,main}.cpp`、`CMakeLists.txt` |
| 5 | テストと見本 | `tests/package_scan/`、`tests/package_viz/test_usage_view.cpp` |
| 6 | 文書: モジュール一覧・README・ドメイン・層・スキーマ・手順・統合設計の実装状況と配置 | `module_list.md` / `.yaml`、`README.md`、`spec/domains/observability.domain.json`、`.anatomia/layers.json`、`spec/data/ergo-packages.md`、`spec/setup/packages.md`、`spec/feature/package-hub.md` |

## scan の置き場所 (判断)

`ergo_package_model` に混ぜず、新モジュール `ergo_package_scan` に分けた。

- 入力が違う: `package_model` はビルドディレクトリ (resolved.json / File API) を読み、scan は兄弟チェックアウトと
  マニフェストを読む。読み取りの範囲規則 (範囲外を開かない) も scan だけのもの。1 モジュール 1 責務に合わせた。
- 依存は `package_viz → package_model / package_scan` の一方向で、`package_scan` と `package_model` は互いに import しない
  (cross-import 禁止)。`package_scan` は `ergo_common` だけに依存し、Pictor / Tela 無しで常時ビルド・テストできる。
- `package_model` 側の変更は resolved.json の `manifest` を読むフィールド 1 つだけ (CLI がマニフェストの場所を解くため)。

## 受け入れ条件 (契約書式)

- C-1 scan_usage(options): `package.json` の `file:` / `workspace:` / `@ludiars/*` 依存、`.gitmodules` + gitlink SHA、マニフェストの `ported_from` / `relations` を辺にし、どの辺にもファイル・行・キーの根拠が 1 つ以上ある — `test_package_scan` (Scan.Extracts* / ResolvesScoped* / ManifestRelations* / EveryEdgeHasEvidence / ReadsGitlink*)
- C-2 scan_usage(options): 公開パッケージ・宣言だけの関係・推測から辺を作らない — `test_package_scan` (Scan.NoEdgesWithoutEvidence)
- C-3 scan_usage(options): `<root>/<path_hint>` と Ergo の宣言した場所の外を開かず、範囲外を指す依存はパスの計算だけで帰属する — `test_package_scan` (Scan.DoesNotReadOutside* / RecordsFilesRead*)
- C-4 scan_usage(options): root 未指定・path_hint 範囲外・チェックアウトなしを理由付きの「未観測」ノードに残す — `test_package_scan` (Scan.KeepsMissing* / WithoutRoot*)
- C-5 build_usage_view / make_usage_view(usage): MUSA / 関連 / 共有 TS / Ergo / ツール / 未観測のグループ、実線 (submodule・依存) と破線 (移植・語彙共有)、辺の無い観測対象も出し、上限ちょうどは 1 枚・超過は分割、どの枚も validate_tela_graph を通る — `test_package_viz` (UsageView.*)
- C-6 ergo_package_viz CLI: `--view usage` / `--scan` を足し、`ergo_package_graphs` が usage.tela も書く — MSVC でのビルドと実チェックアウトでの手元実行 (下の検証)

Augur の契約 (`augur.contracts.json` と述語モジュール) は用意できない。Augur の `contract-wrap` は TypeScript の関数を
構文解析して包む仕組みで、C++ の関数は対象にならない。上の受入条件は ctest のテスト名で裏づける。

## 再利用探索の採否

| 候補 | 採否 | 理由 |
|---|---|---|
| `ergo_common::jsonm` | 採用 | package.json とマニフェストの値を読む。位置を持たないので行番号だけ別の走査 (`JsonLineIndex`) で付けた |
| `ergo_package_model` の `json_access` (detail) | 不採用 | package_model の内部部品で、他モジュールから include すると cross-import になる。必要な取り出しは数行なので scan 側に書いた |
| `ergo_package_model` の resolved.json の `observe` | 不採用 (図の入力としては) | resolved.json の relations は `from:kind` の文字列で行番号を持たず、`ported_from` の行も無い。根拠を付けるためマニフェストそのものを読む |
| `ergo_package_viz` の fit 手順 (簡約・焦点・分割) | 採用 | P3 も同じ規則で上限に収める (`fit_view` をそのまま呼ぶ) |
| CLI の `version_probe` (git の起動) | 不採用 | gitlink は index を直接読む。git を起動しないので範囲外 (worktree の git ディレクトリ) を読まない規則を守れる |
| Anatomia / Concordia / Revisor への問い合わせ | 不採用 | N8 で初版は兄弟チェックアウトだけ。ネットワークと認可は S7 |

## 検証

| 何を | 結果 |
|---|---|
| MSVC (VS 2022, x64, Debug) で `test_package_scan` / `test_package_viz` / `test_package_model` / `ergo_package_viz_cli` をビルド | 成功 (警告は既存の `json_min.h` の C4819 のみ) |
| テストの実行 (ctest) | 未実施 (委託の指示で、明示の指示が無い限りテストを実行しない) |
| 実チェックアウトでの手元確認 (`--scan` / `--view usage --root <兄弟の親>`) | 実施。Thaleia → Lapilli (submodule + file:、gitlink は `git ls-files -s` と一致)、tools/ergo → Lapilli、Ergo → Lapilli、ergo_resource → Clio (移植・破線)、ergo_resource → Curare (語彙共有・破線)。Clio / Curare / Ars-Musa はルートに package.json も .gitmodules も無く辺なし (存在だけ表示)。Terpsichore は package.json の name だけ。図は 9 ノード 5 辺で 1 枚 |
| GCC でのビルド | 未実施。designated initializer を使わず、`std::tolower` は `unsigned char` で呼び、`u8string` の型差を吸収した |
| `tela_view` での画面確認 | 未実施 (画面確認はしない指示) |
| 秘匿語の辞書照合 | 提出前にファイルとコミットで 0 件を確認 |

## 復旧

- P3 だけ止める: `-DERGO_BUILD_PACKAGE_SCAN=OFF` (このとき `ergo_package_viz` と CLI もビルドされない)。
  P1 / P2 も要らなければ従来どおり `-DERGO_BUILD_PACKAGE_VIZ=OFF`。既存モジュールはどれも新モジュールに依存しない。
- 戻すときはこの PR を revert する。M0 のビルド結果、resolved.json の形式には影響しない。
