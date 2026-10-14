# ergo_tela_graph モジュール定義

## 概要

Ergo の可視化の共通出口。 配置済みの関係図 (グループ・ノード・有向の辺) を値型
`ergo::tela_graph::Graph` で受け取り、 Tela の `TELA_GRAPH 1` ファイルとして書き出す。
書き出したファイルは Tela の既存ホストでそのまま開ける。

```sh
tela_view    --font <file.ttf> --graph <file>                       # 自前のウインドウ
tela_overlay --font <file.ttf> --graph <file> --attach <target>     # 作業中のウインドウへ重ねる
```

Tela はレイアウトを計算しない (Tela `spec/feature/graph-view.md`)。 ノードの位置と
辺の経路は図を作る側が決める。 このモジュールは形式・上限・検査だけを持ち、
何を描くかは知らない。 描画パイプライン (`ergo_render_viz`) を最初の利用者とし、
今後のパッケージ構成の可視化も同じ出口を使う。

Tela をプロセス内で使うホスト向けに、 `tela::Graph` への変換
(`ergo_tela_graph_tela`) も用意する。 こちらは `Tela::Graph` ターゲットがある
構成だけでビルドされる。

## カテゴリ

システム

## 所属ドメイン

`structure-visualization` (構成の可視化。 2026-10-14 の切り直しで observability から移動)。

## 必要なデータ

- ホストが組み立てた `Graph` (グループ ≤ 8、 ノード ≤ 128、 辺 ≤ 256、 経路点 ≤ 4096)。

## 依存

- C++17 標準ライブラリ (`<filesystem>` / `<fstream>` / `<sstream>`)
- `ergo_tela_graph_tela` のみ: Tela (`Tela::Graph`、 C++20)
- テスト: mini-gtest (`ergo_gtest_main`)

## 作業

### 入力

- `validate_tela_graph(graph, &error)` — Tela が受け付ける条件の検査。
- `to_tela_graph_text(graph, &error)` — `TELA_GRAPH 1` のテキスト。
- `save_tela_graph(path, graph, &error)` — ファイルへ書く。
- `to_tela_graph(graph)` — `tela::Graph` へ変換 (Tela がある構成のみ)。

### 出力

- LF 改行・BOM なしの `TELA_GRAPH 1`。 フィールドは引用符で囲み、 `"` と `\` を
  エスケープする。 改行などの制御文字は空白にする (1 レコード 1 行の形式を守る)。
- 座標は小数 2 桁 (末尾の 0 は落とす)。 ロケールに依存しない。

### タスク

- Tela の契約 (Tela `spec/contracts.md` SPEC-TL-GRAPH) と同じ条件で検査し、
  満たさなければ **何も書かない** (Tela も不正なファイルを部分的に使わない)。
  条件: 寸法が正、 座標が有限で ±100000 以内、 ID が空でなく 256 バイト以内で重複なし、
  ノードは既知のグループ、 辺の両端は既知のノード、 辺は 2 点以上、 4 MiB 以内。
- ファイルは隣の一時ファイルへ書いてから置き換える。 失敗しても既存ファイルを壊さない。

## テスト

| テスト | 確かめること |
|---|---|
| `test_tela_graph_writer` | レコード順・エスケープ・制御文字・数値表記・拒否条件・上限・書き込み失敗時に既存ファイルが残ること |
| `test_tela_graph_adapter` (Tela がある構成のみ) | Ergo が書いたファイルを Tela 本体の `tela::load_graph` が受け付け、 プロセス内変換と同じ図になること |

## 未確認

- Tela の画面 (`tela_view --graph`) での見え方は未確認 (ファイルの受理はテストで確認済み)。
