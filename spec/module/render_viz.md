# ergo_render_viz モジュール定義

## 概要

Pictor の描画パイプラインを Tela の関係図にする可視化層。 パス列・アタッチメント
(描画先と入力テクスチャ)・ポストプロセス・フレームの CPU 段と GPU 時間を 1 枚の図に
並べ、 どのパスが何を書いて何を読むか、 どこが重いかを Tela で見られるようにする。
出口は `ergo_tela_graph` (`TELA_GRAPH 1` ファイル、 または `tela::Graph`)。

3 段に分かれる。

| ターゲット | 役割 | ビルド条件 |
|---|---|---|
| `ergo_render_viz` | スナップショット (`PipelineSnapshot`) と配置 (`layout_pipeline`) | 常時 (Pictor 不要) |
| `ergo_render_viz_pictor` | `pictor::PipelineProfileDef` / `pictor::FrameStats` → スナップショット | `pictor` ターゲットがあるとき |
| `ergo_render_viz` (CLI、 ターゲット名 `ergo_render_viz_cli`) | プロファイル JSON / プリセット → `.tela` ファイル | `pictor` ターゲットがあるとき |

`ergo_render` (Pictor 横断オーケストレーション層) とは独立で、 互いに import しない。
Pictor の公開型だけを読み、 GPU API の型には触れない (Pictor の描画方式が増えても
影響を受けない)。

### 使い方

構造だけを見る (CLI):

```sh
ergo_render_viz --preset High --out high.tela
ergo_render_viz --profile Pictor/profiles/high.profile.json --out high.tela
tela_view --font <file.ttf> --graph high.tela
```

実行中のレンダラの時間付きで見る (ホスト):

```cpp
auto snap  = ergo::render_viz::snapshot_from_pictor(profile, &profiler.get_frame_stats());
auto graph = ergo::render_viz::layout_pipeline(snap);
ergo::tela_graph::save_tela_graph("pipeline.tela", graph);
```

## カテゴリ

システム

## 所属ドメイン

`observability` (診断出力・可視化)。

## 必要なデータ

- Pictor のパイプライン定義 (`PipelineProfileDef`)。 アタッチメント宣言が空なら
  Pictor の既定アタッチメントを宣言済みとして扱う (Pictor の実行時と同じ)。
- 任意: Pictor の `FrameStats` (CPU 段の時間・パス種別ごとの GPU 時間・フレーム時間)。

## 依存

- `ergo_tela_graph`
- `ergo_render_viz_pictor` / CLI のみ: Pictor (`pictor`、 C++20)
- テスト: mini-gtest (`ergo_gtest_main`)

## 作業

### 入力

- `snapshot_from_pictor(profile, stats)` — Pictor の定義・計測値の写し。
- `layout_pipeline(snapshot, options)` — 配置済みの関係図。

### 出力

図の行 (上から):

1. **CPU 段** — DataUpdate → Culling → Sort → BatchBuild → CommandEncode。 時間付き。
   計測値を渡したときだけ出す。 最後の段から最初のパスへ、 左の通路を通って繋ぐ。
2. **アタッチメント** — パスが参照する順。 形式・表示先・深度を添える。
3. **render pass** — 実行順に左から右へ。 隣り合うパスを実線で繋ぐ。
4. **エフェクト** — ポストプロセス・スタック。 最初の PostProcess パスの下から右へ。 無効は「(無効)」。

辺: パス → アタッチメント (書き込み) は実線、 アタッチメント → パス (読み取り) は破線。
経路は縦横の折れ線で、 長さ 0 の区間を作らない。

Tela の切り替えボタン (グループ): CPU 段 / 影・深度 / 不透明・半透明 / ポストプロセス /
計算・カスタム / アタッチメント。 ノードの無いグループは出さない。

### タスク

- **設定ミスを図で見つける**: パスが参照するのに宣言の無いアタッチメントは「未宣言」と
  表示して残す。 どのパスも参照しない宣言も図に残す。
- **重い所を示す**: Pictor の `FrameStats` は GPU 時間を種別単位でしか持たないので、
  パスには「種別合計」として添え、 最大の種別に「★最重」を付ける。 Custom 種別は
  計測の受け皿が無いので時間を出さない。
- 同名のパスやエフェクトが並んでも、 ノード ID に `#2` などを付けて衝突させない。
- 配置は決定的 (同じ入力から同じ図)。

## テスト

| テスト | 確かめること |
|---|---|
| `test_render_viz_layout` | 実行順の並び・行の上下関係・実行順の辺・書き込み実線 / 読み取り破線と着地点・縦横の折れ線・エフェクトの連鎖・未宣言 / 未参照アタッチメント・最重の印・CPU 段の有無と通路・空グループを出さない・同名 ID・決定性・Tela の検査を通ること |

CLI は Pictor の組み込みプリセット (Low / Mid / High / Ultra) と
`Pictor/profiles/high.profile.json` から書き出せることを手元で確認した (自動テストではない)。

## 未確認

- Tela の画面での見え方 (文字の収まり・線の重なり) は未確認。
- 実行中のレンダラの `FrameStats` を渡した図は、 合成値のテストのみで実機の値では未確認。
- GCC でのビルドは未確認 (MSVC で確認)。
