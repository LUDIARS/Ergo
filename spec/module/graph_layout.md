# ergo_graph_layout モジュール定義

## 概要

座標を持たない抽象グラフ (ノード・グループ・有向辺) を層別に配置し、 `ergo_tela_graph` の
配置済み関係図 (`tela_graph::Graph`) にする汎用の配置層。 Tela はレイアウトを計算しないので、
図を作る側がこのモジュールで位置と辺の経路を確定させる。 最初の利用者は `ergo_package_viz`
(パッケージ・ハブの P1 / P2)。 `ergo_render_viz` の配置は寄せない (spec/feature/package-hub.md U7)。

配置の性質:

| 性質 | 内容 |
|---|---|
| 向き | 依存の向きは左 (使う側) → 右 (使われる側) |
| 列 | 最長経路。 循環は ID 順の深さ優先で見つけた戻り辺を逆向きに数えて切る (辺は消さず、 向きも保つ) |
| 列内の順 | ID 順から始め、 重心法 2 往復 (右向き・左向きを 2 回ずつ)。 同じ重心は ID 順で固定 |
| 長い辺 | 2 列以上をまたぐ辺は途中の列に通り道 (仮の枠) を取り、 ノードを横切らない |
| 経路 | 縦横の折れ線。 列間の通路に辺ごとの縦の通り筋を割り当てる。 座標は 1/4 px に丸め、 長さ 0 の区間と一直線上の中間点を除く |
| 寸法 | ノードの高さは札の行数 (`" | "` 区切り) から。 キャンバスは ±100000 内 (超えるときは等倍に縮める) |
| 決定性 | 同じ入力から同じバイト列。 ハッシュ表の走査順に依存しない |

自己ループと端点の無い辺は描けないので落とし、 `LayoutResult::dropped_edges` に数える。

### 使い方

```cpp
ergo::graph_layout::LayoutInput in;
in.project = "Ergo";
in.title   = "依存";
in.groups  = {{"core", "コア", {66, 133, 244}}};
in.nodes   = {{"app", "core", "app | EXECUTABLE"}, {"lib", "core", "lib"}};
in.edges   = {{"app", "lib", false}};
auto result = ergo::graph_layout::layout_layered(in);
ergo::tela_graph::save_tela_graph("deps.tela", result.graph);
```

## カテゴリ

システム

## 所属ドメイン

observability (`spec/domains/observability.domain.json`)。 層は shared (`.anatomia/layers.json`)。

## 必要なデータ

なし (入力は呼び出し側の値)。

## 依存

- `ergo_tela_graph` (出力の型)。 Tela / Pictor には依存しない。

## 変数

`LayoutOptions`: `node_width` (240) / `line_height` (18) / `node_padding` (10) / `column_gap` (96、 列間の通路) /
`row_gap` (24) / `margin` (32) / `dummy_height` (8、 通り道の高さ)。

## 作業

### 入力

`LayoutInput` (project / title / groups / nodes / edges)。 グループは並べたい順に渡す。

### 出力

`LayoutResult` — `graph` (配置済み関係図。 ノードの無いグループは出さない。 辺の ID は `e0`, `e1`, …)、
`dropped_edges`、 `columns`。

### タスク

1. `layering.cpp` — ID 順位、 戻り辺の検出 (反復の深さ優先)、 最長経路の列 (Kahn 法)。
2. `ordering.cpp` — 通り道の枠を挿入し、 重心法 2 往復で列内の順を決める。
3. `routing.cpp` — 端点 (相手の順で上から)、 通路の通り筋、 折れ線、 丸めと単純化。
4. `layered_layout.cpp` — 列の x、 枠の y、 縮小、 出力の組み立て。

## テスト

`tests/graph_layout/test_graph_layout.cpp`: 札の行数と高さ、 左 → 右と最長経路の列、 バイト単位の決定性、
縦横の折れ線と長さ 0 なし、 端点がノードの辺に付く、 長い辺と疑似乱数グラフでノードを横切らない、 循環を落とさず
向きを保つ、 自己ループ・端点不明を落とす、 同順位は ID 順、 未使用グループを出さない、 ±100000 内への縮小。
