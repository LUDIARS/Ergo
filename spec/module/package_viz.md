# ergo_package_viz モジュール定義

## 概要

`PackageModel` (ergo_package_model) を Tela の関係図にするパッケージ・ハブの可視化層
(spec/feature/package-hub.md §5、 段階 S3)。 配置は `ergo_graph_layout`、 出口は `ergo_tela_graph`。
依存は `package_viz → package_model / graph_layout → tela_graph` の一方向で、 Pictor / Tela には依存しない。

| 図 | ノード | グループ (≤ 8) | 辺 |
|---|---|---|---|
| **P1** `--view packages` | パッケージ (host・Ergo・link・tool・第三者・observe) | host / Ergo / 描画 / 可視化 / ツール / 第三者 / 観測: MUSA / 観測: 関連 | 実線: link の `requires`、 File API の実リンクをパッケージ単位に畳んだもの。 破線: `ported_from`、 observe の `relations` |
| **P3** `--view usage` | 観測対象 (MUSA 系・関連)・共有 TS (Lapilli・`@ludiars/*`)・Ergo と移植/語彙共有の根拠を持つモジュール・`tools/ergo` | Ergo / ツール / MUSA / 関連 / 共有 TS / 未観測 | 実線: submodule・`file:` / `workspace:` / `@ludiars/*` 依存。 破線: `ported_from`・`relations`。 入力は `ergo_package_scan` の UsageModel |
| **P2** `--view targets` | ターゲット (既定の除外を除く) | application / presentation / domain-logic / infrastructure / shared (`.anatomia/layers.json`) + 外部パッケージ + 第三者 + 循環 | 直接の依存 (実線)。 循環 (強連結成分 ≥ 2) は `cycle` グループへ |
| **P4** `--view host` | ホスト 1・Ergo 1・依存・更新 (タグ・main)・審査中・作業中 (local PR / run)・落ちた検査 | host / Ergo / 依存 / 更新: タグ・main / 更新: 審査中・作業中 / 検査 | 実線: host → Ergo・依存。 破線: 依存 → 更新・審査中・検査 (統合設計 §5.1 の点線は Tela に無いので破線にし、 検査のグループの色で見分ける)。 入力は `package_model::HostState` (spec/feature/package-manager.md §5.1、 段階 W6) |

P1 の札: 名前 (コード) / 版 (出所付き: `revisor` / `git` / `cmake`、 無ければ「版 不明」) / SHA7 (HEAD、 無ければ pin) /
状態と経路 / 機能 / 印 (`★drift:<値>` `dirty` `上書き` `宣言漏れ` `未観測` `未初期化`)。 link パッケージの描画 / 可視化の
振り分けはマニフェストの `group` (`render` / `viz`) を見る。 P2 の札: 名前 / 型 / パッケージ · 層 / `★宣言漏れ`。
P3 の札: 名前 (コード) / 未観測なら理由と宣言の group / package.json の name / 出る辺の根拠 (`→ 先 [種類] ファイル:行 キー (注)`、
3 件まで、 残りは「…他 N 件」)。 観測対象は辺が無くても出す (統合設計 N5)。 P3 は `make_usage_view` (`usage_view.h`) で作り、
`make_view` は P3 を受け付けない (scan が要るため)。

P4 の札 (`host_labels`、 欠けた値は `—`): host = コード (resolved.json の `host.id` → status.json → 報告) / `HEAD <SHA7>` /
`モード <M0〜M2>` / テスト (`3/5`、 `1 失敗 (5 中)`、 状態の source が unknown なら `未実行`)。 Ergo = 版 (報告の `pin.tag`) /
pin SHA7 / 遅れ (`main +18` `新タグ v0.10.0` `off-main`、 無ければ `なし`、 観測できなければ `未観測`)。 依存 = 名前 (コード) /
版 · pin / 経路 · drift / 機能 / 印 (`★drift` `dirty` `off-main` `未観測` `非公開`)。 依存は resolved.json の link のうち
`absent` でないものと、 報告の依存 (Ergo 自身を除く) の和。 更新 = `v2.3.0 (41 先)` / `main +41 (うち merged 3)` (依存ごとに 1 ノード)。
審査中・作業中 = `#128 Pf` / `#131 AI` / `#133 人` / `run AI` (由来の不明は `不明`)。 `kHostReviewNodes` (12) を超えたら
由来ごとに `review:origin:<由来>` の 1 ノードへ畳み、 件数 (審査中・作業中の内訳) と番号 6 件までを札に、 畳んだことを題に出す。
検査 = resolved.json の findings のうち error / warning の ID と対象 (record は描かない)。 対象が依存なら依存から、 でなければ
host から辺を引く。 報告の `sources.*` が `unavailable` なら題に「Revisor に届かず」などを添える (更新なしと取り違えない)。
P4 は `make_host_view` (`host_view.h`) で作り、 `make_view` は P4 を受け付けない。

### 上限に収める手順 (`fit.h`、 決定的・収まった所で止める)

1. 既定除外 (組み立て時)。
2. 推移簡約 — 実線だけ、 (from, to) 順に 1 本ずつ判定。 消した本数を題に添える。
3. 焦点と畳み込み — targets 図は host と Ergo を展開し他パッケージを 1 ノード (`pkg:<名前>`) に畳む (`--expand` で
   展開を足す)。 `--focus` (パッケージ / 層 / ターゲット) は当たったノードと 1 歩の隣だけを残す。 `--focus` / `--expand`
   を指定したときは収まっていてもかける。
4. 分割 — (グループの列順, ID) で並べて 1 枚 128 ノード・256 辺の手前で次の枚へ。 枚をまたぐ辺は索引図 (`out.index.tela`)
   に枚同士の辺と本数の札で載せる。 配置後の検査 (経路の点数など) に落ちる枚があれば 1 枚のノード数を半分にして分け直す。

どの段でも `validate_tela_graph` を通らない 1 枚は返さず、 CLI は全枚を検査してから書く (1 枚でも落ちれば何も書かない)。

### 使い方

```sh
cmake --build build --target ergo_package_graphs     # build/graphs/packages.tela・targets.tela・usage.tela・host.tela
ergo_pkg status --build build --junit build/ctest-junit.xml   # P4 の入力 build/ergo-packages.status.json
ergo_package_viz build --view host --host <ホストのリポ> --out host.tela
ergo_package_viz build --view usage --root <兄弟チェックアウトの親> --out usage.tela
ergo_package_viz build --scan --root <dir>            # 観測の辺を根拠付きで表示
ergo_package_viz build --view packages --out packages.tela
ergo_package_viz build --view targets --focus ergo --out targets.tela
ergo_package_viz build --view targets --expand pictor --out targets.tela
ergo_package_viz build --check                         # PKG-17 宣言漏れで終了コード 1 (CI の 1 段)
tela_view --font <file.ttf> --graph packages.tela
```

CLI の終了コード: 0 成功 / 1 宣言漏れ (`--check`) / 2 引数・入力の誤り / 3 図にできない・書けない。
`--no-version-probe` で外部コマンドによる版の照会を止める (resolved.json の project(VERSION) のまま)。

## カテゴリ

システム

## 所属ドメイン

structure-visualization (`spec/domains/structure-visualization.domain.json`、 2026-10-14 の切り直しで observability から移動)。 層は infrastructure (CLI は application)。

## 必要なデータ

`ergo_package_model` が読める構成済みのビルドディレクトリ。 targets 図と `--check` は File API の返答が必須、
packages 図は無くても書く (題に「実リンク未取得」)。

## 依存

- `ergo_package_model`、 `ergo_graph_layout` (→ `ergo_tela_graph`)。
- CLI の版の照会は実行時に `revisor` / `git` があれば使う (無ければ黙って次へ)。

## 変数

`ViewOptions`: `view` / `focus` / `expand` / `include_tests` / `limits` (8 / 128 / 256) / `layout`。
`kHostReviewNodes` (P4 の審査中・作業中を 1 件 1 ノードで描く上限、 12)。

## 作業

### 入力

`PackageModel`、 `ViewOptions`。

### 出力

`FitResult` — `graphs` (1 枚または分割した枚)、 `split`、 `index`、 `steps`。 `output_files` が書き出すパス
(`out` か `<stem>.N.tela` + `<stem>.index.tela`) を決める。

### タスク

1. `package_view` / `target_view` — P1 / P2 の抽象グラフと札 (`labels`)、 循環の検出 (反復 Tarjan 法)。
2. `reduction` — 推移簡約。
3. `collapse_focus` — 畳み込みと焦点。
4. `split` — 分割と索引図。
5. `fit` — 手順の適用、 配置、 検査、 出力名。
6. `host_view` / `host_labels` — P4 の抽象グラフ・由来ごとの畳み込みと札。
7. CLI `tools/package_viz/` — 引数 (`cli_options`)、 版の照会 (`version_probe`)、 観測 (`usage_command`)、
   P4 の入力の場所 (`host_command`: `--resolved` / `--report` か `<host>/.ergo/watch-report.json` / `--status`)、 `main`。

## テスト

`tests/package_viz/test_package_viz.cpp` (ergo_package_model の合成見本を使う): P1 のノード・札・辺、 P1 が
上限内で検査を通る、 P2 の層グループと除外、 循環グループ、 推移簡約 (重複・破線・循環)、 畳み込み (複数ターゲットを
1 ノード、 `--expand`)、 焦点と当たらない焦点、 128 ノードちょうどは 1 枚、 129 ノードは分割と索引と出力名、 辺の上限超過は
簡約の後に分割、 分割の枚ごとの上限、 決定性、 File API が無いときの targets 図。
`tests/package_viz/test_host_view.cpp` (P4、 見本は `tests/package_model/fixtures/host_state/`): resolved + 報告 + JUnit 由来の
状態から作った図のノード・札・辺の種類、 `validate_tela_graph` 通過、 審査中 20 件が由来ごとに 4 ノードへ畳まれ上限ちょうどは
畳まない、 入力が欠けても描く (札 `—`、 状態だけ欠けた・未実行)、 決定性 (同じ入力で同じ `.tela` のバイト列)。
`tests/packages/test_package_host_view.cmake`: `ergo_pkg status` → `ergo_package_viz --view host` の通し (欠けた入力・決定性)。
`tests/package_viz/test_package_viz_tela.cpp` (`Tela::Graph` がある構成だけ): 書いた P1 / P2 を Tela 本体の
`tela::load_graph` が受理し、 プロセス内変換と同じ図になる。

## 未確認

- `tela_view` での見え方 (画面確認はしていない)。
- 実ホスト (Pictor 実ソース) の File API での帰属と図。
- `revisor version show` の出力書式は README のコマンド名以上を確かめていない (最初の `MAJOR.MINOR.PATCH` を拾う)。
