# ergo_stickfig — 棒人間幾何学生成

## 1. 概要

棒人間 (stick figure) を「頭 = 球 + 四肢/胴 = カプセル」の連結プリミティブとして
生成する純粋幾何学モジュール。 `ergo_render` の `StageRenderer` が描く軽量
プリミティブ群に流し込める CPU メッシュ (頂点 + インデックス) を組み立てる。

描画・GPU アップロードは行わない (Vulkan 非依存)。 メッシュの GPU 化と描画は
consumer (ゲーム / `ergo_render`) の責務。

## 2. カテゴリ

ゲームオブジェクト (プロシージャル幾何生成)

## 3. 所属ドメイン

`ergo_stickfig` (`include/ergo/stickfig/`, `src/stickfig/`, `tests/stickfig/`)

## 4. 必要なデータ

- `StickFigureParams` — 全高 / 腕幅 / 四肢半径 / 頭半径 / テッセレーション分割数

## 5. 依存

なし。 他の `ergo_<domain>` を import しない。 出力頂点 `Vertex` は
`ergo::render::StageVertex` (pos[3] + normal[3]) とバイナリ互換にしてあり、
描画側はコピーで `StageRenderer::upload_mesh()` に渡せる。

## 6. 変数 / API

| 関数 | 役割 |
|------|------|
| `generate_sphere(radius, segments, rings)`         | 頭部用 UV 球メッシュ |
| `generate_capsule(radius, length, segments, caps)` | 四肢用カプセル (円柱 + 半球キャップ、 +Y 軸) |
| `generate_stick_figure(params)`                    | 頭 + 胴 + 両腕 + 両脚を組んだ 6 パーツ |

`generate_stick_figure` は各パーツを `StickPart { name, mesh, color[4],
model[16] }` で返す。 `model` は列優先 4x4 で `StageDrawable.model` と同じ並び。

## 7. 作業 (入力 / 出力)

- 入力: `StickFigureParams`
- 出力: `std::vector<StickPart>` (= メッシュ + 色 + 配置行列の列)

## 8. テスト

`tests/stickfig/test_stick_figure.cpp`:
- 球 / カプセルの頂点・インデックス整合 (index 範囲内、 三角形数が 3 の倍数)
- 法線が概ね単位長
- カプセル全長 = 円柱長 + 2 * 半径、 半径上限の検証
- 退化パラメータ (segments/rings 最小未満) のクランプ
- `generate_stick_figure` が 6 パーツを返し、 height に応じて頭部 y が動く

## 9. 付随 Web ツール

`tools/ergo/src/plugins/stickfig/` — 同じジョイント式で関節 + ボーン + 頭部の
構造化スペックを JSON で返すプレビュー用プラグイン (`GET /stickfig/api/preview`、
`POST /stickfig/api/generate`)。 数値式は C++ 側と同一なので、 比率を変える際は
両方を同じ PR で更新すること。
