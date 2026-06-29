# ergo_vector — SVG パス → ポリゴン化 → 体積 3D メッシュ + 頂点アニメーション (設計書)

> 状態: 設計 (2026-06-01、ThorVG ラスタ案から全面改定)。実装は Codex 委託予定。
> 区分: 新規 Ergo モジュール `ergo_vector`。
> 関連: [ui_layout.md](./ui_layout.md) (vector ノードのバックエンド), [render.md](./render.md), Pictor 3D パイプライン / `TextSvgRenderer` (パス表現の再利用元), KuzuSurvivors HUD (最初の consumer)。

## 0. 方針 (なぜ独自実装か)

ThorVG 等の SVG ラスタライザは「SVG → 2D ピクセル → テクスチャ」になり、**3D と融和しない**(常に板ポリ)。本モジュールは SVG を **パス → 多角形分割 (テッセレーション) → Z 押し出し (extrude) で体積メッシュ → 3D パイプラインで描画**し、**頂点レベルでアニメーション**する独自実装にする。これにより SVG 図形・文字を 3D シーンに溶け込ませ、ライティング/深度/遠近/傾き/頂点モーフを効かせる。

外部ラスタライザ (ThorVG) は**使わない**。SVG パースも**完全自前**。Pictor は既に `TextSvgRenderer` でグリフ輪郭をパスコマンド (MOVE/LINE/QUAD/CLOSE) に分解しているので、**パス表現はそれと共通化**し、SVG 図形も文字も同一の「パス → 平坦化 → テッセレーション → extrude → 3D メッシュ」経路に乗せる (= 3D 押し出し文字も自然に得られる)。

### スコープ
- **やる**: SVG パース (サブセット)、ベジェ平坦化、塗り/線のテッセレーション、Z extrude による体積メッシュ生成 (前面/背面キャップ + 側壁 + 法線)、3D メッシュとしての提供、頂点アニメーション (ノード affine / 頂点モーフ / extrude 深度)、data-binding。
- **やらない**: フル SVG 1.1 互換 (必要サブセットのみ)、Lottie、ラスタ経路、SVG オーサリング GUI (それは ui_layout のエディタ側)。

## 1. パイプライン

```
*.svg ─parse─> VectorPath[] (Pictor パスコマンド互換: Move/Line/Quad/Cubic/Close + fill/stroke/transform)
        │
        ├ flatten   ベジェ → ポリライン (適応的、許容誤差 tol)
        ├ tessellate 塗り: 輪郭(穴含む) → 三角形 (earcut.hpp、even-odd/nonzero 対応)
        │            線:   ストローク → 四角ストリップ (join/cap)
        ├ extrude    2D 三角形群 + 輪郭 → 体積メッシュ:
        │              front cap (z=+d/2) + back cap (z=-d/2) + side walls (輪郭押し出し)
        │              法線生成 (cap=±Z, wall=輪郭法線)、depth はノード毎に可変
        └ → VectorMesh { vertices(pos/normal/uv/color), indices }  ← 3D メッシュ
                │
        VectorScene (ノードツリー: 各ノードに mesh + local transform + material + アニメ状態)
                │  update(dt, BindContext): affine / 頂点モーフ / extrude 深度を適用
                └ emit → Pictor 3D 描画 (model 行列, 深度テスト, ライティング) = 3D と融和
```

- 平坦化の許容誤差は LOD パラメータ。アニメで頂点モーフするノードは「全状態を同一トポロジ(同一頂点数・対応順)でテッセレート」する制約を設け、頂点単位 lerp を可能にする (§4)。

## 2. モジュール構成 (Ergo 規約準拠、SOLID/SRP でファイル分割)

| 項目 | 値 |
|---|---|
| name | `ergo_vector` |
| header_dir | `include/ergo/vector/` |
| source_dir | `src/vector/` |
| test_dir | `tests/vector/` |
| spec | `spec/module/vector.md` (本書) |
| cmake_option | `ERGO_BUILD_VECTOR` |
| tool_plugin | null (編集 UI は ui_layout 側) |
| status | preview |

`module_list.yaml`/`.md` 追記 (CLAUDE.md「新規モジュール追加手順」準拠)。

### ファイル分割 (1 ファイル 1 責務)
- `svg_parser.{h,cpp}` — SVG XML サブセット → `VectorPath[]`
- `path.{h,cpp}` — パス表現 (Pictor パスコマンドと相互変換)
- `flatten.{h,cpp}` — ベジェ → ポリライン
- `tessellator.{h,cpp}` — 塗り/線 → 三角形 (earcut ラップ)
- `extruder.{h,cpp}` — 2D → 体積メッシュ + 法線
- `vector_mesh.{h,cpp}` — メッシュ POD (pos/normal/uv/color, indices)
- `vector_scene.{h,cpp}` — ノードツリー / transform / アニメ状態 / update
- `morph.{h,cpp}` — 同一トポロジ頂点モーフ
- `bind.{h,cpp}` — data-binding (式評価は ui_layout 側と共有 or 最小実装)

### 依存ライブラリ
- **earcut.hpp** (mapbox, ISC, ヘッダオンリー) を `third_party/earcut/` に vendoring (固定 commit pin)。穴(複数リング)対応。ThorVG のような重い vendoring は不要。
  - 自己交差/even-odd で earcut が不十分なら `libtess2` を fallback 候補として記載 (初期は earcut + 単純塗りで可)。
- それ以外は標準 C++ のみ。Pictor 非依存 (メッシュ生成まで)。

## 3. SVG サブセット (パーサが扱う範囲)
- 要素: `path`(d), `rect`, `circle`, `ellipse`, `line`, `polyline`, `polygon`, `g`(group + transform)。
- path `d`: M/m L/l H/h V/v C/c Q/q (S/s T/t は任意) Z/z。アーク A は後回し可。
- 属性: `fill`, `stroke`, `stroke-width`, `fill-rule`(nonzero/evenodd), `opacity`, `transform`(matrix/translate/scale/rotate), `id`。
- `id` は data-binding とアニメ対象指定に使う。

## 4. アニメーションモデル (段階的、上から含む)
1. **ノード affine (2.5D 配置)**: 各ノードに local transform (T/R/S) + 親基準。3D 空間に置き深度/傾き/遠近を効かせる。バー類は子ノードの scale_x で表現。
2. **頂点モーフ**: パスの複数「状態」を**同一トポロジ**でテッセレート (頂点数・対応順を一致させる前処理)。状態間を 0..1 で lerp → グニャグニャ変形。Lottie 的形状アニメをポリゴンで実現。
3. **extrude 深度アニメ**: depth を時間/値で変える (飛び出し演出)。
- トリガ/補間は `VectorScene::update(dt, BindContext)`。状態遷移 (被ダメ点滅・レベルアップ強調) は BindContext のフラグ/値で駆動。

## 5. 公開 API (include/ergo/vector/)

```cpp
namespace ergo::vector {

struct VectorMesh {                         // 3D メッシュ
  std::vector<Vertex> vertices;             // pos(x,y,z) / normal / uv / rgba
  std::vector<uint32_t> indices;
};

struct TessOptions { float flatten_tol = 0.25f; FillRule rule = FillRule::NonZero; };
struct ExtrudeOptions { float depth = 0.0f; bool front=true, back=true, walls=true; };

// パス → メッシュ (単発・低レベル)
VectorMesh build_mesh(const std::vector<VectorPath>& paths,
                      const TessOptions&, const ExtrudeOptions&);

// SVG ドキュメント → ノードツリー (高レベル)
class VectorScene {
public:
  static std::unique_ptr<VectorScene> load_svg_file(const std::string& path);
  static std::unique_ptr<VectorScene> load_svg_data(std::string_view svg);

  // Pictor TextSvgRenderer 等から得たパスを直接ノード化 (3D 文字用)
  void add_path_node(std::string id, std::vector<VectorPath>, ExtrudeOptions);

  // --- アニメ/バインド ---
  void set_node_transform(std::string_view id, const Transform& t);
  void set_scale_x(std::string_view id, float s01);           // バー
  void set_color(std::string_view id, Rgba);
  void set_opacity(std::string_view id, float a01);
  void set_extrude_depth(std::string_view id, float depth);
  void add_morph_target(std::string_view id, const std::vector<VectorPath>& state); // §4-2
  void set_morph_weight(std::string_view id, float w01);

  void update(float dt /*, optional BindContext*/);

  // --- 出力: 3D メッシュ列 (Pictor 非依存) ---
  // consumer (ui_layout / KS) が Pictor 3D パイプラインへ流す。
  struct DrawItem { const VectorMesh* mesh; Mat4 model; MaterialParams mat; };
  void collect(std::vector<DrawItem>& out) const;

  bool dirty() const;   // 再テッセレート/再アップロードの要否
};

} // namespace
```

- **Pictor 連携は consumer 側**。`ergo_vector` は `VectorMesh` + model 行列 + マテリアルまで。GPU 頂点バッファ化/描画は ui_layout の RenderAdapter か KS の 3D レイヤが行う (レイヤ単方向、ergo_vector は Pictor 非依存)。
- 再テッセレートは形状変化時のみ。affine/色/depth/morph-weight は頂点再生成不要 (model 行列・モーフ補間・uniform で済むものは GPU 側)。

## 6. 文字 (3D 押し出しテキスト)
- Pictor `TextSvgRenderer` がグリフ→パスコマンドを出すので、その出力を `add_path_node` に渡せば**文字も extrude 体積メッシュ**になり 3D 文字が得られる。HUD のタイマー/キル/Lv もこの経路。
- パス表現を Pictor と揃える (相互変換 or 共通型) ことを実装時に確認。

## 7. テスト (tests/vector/、Vulkan 非依存)
- SVG パース: 各要素・transform・fill-rule の代表ケースで VectorPath が期待通り。
- 平坦化: 円を tol 別に平坦化し頂点数が単調、面積誤差が許容内。
- テッセレーション: 穴あき多角形 (例: ドーナツ/even-odd) で三角形被覆が穴を除外、総面積一致。
- extrude: depth>0 で頂点数 = cap×2 + wall、法線が ±Z と側方を向く、閉じた多様体。
- モーフ: 2 状態 (同一トポロジ) を weight 0/0.5/1 で補間し中間頂点が線形。
- collect: ノード transform が model 行列に反映。

## 8. 受け入れ基準
1. `ERGO_BUILD_VECTOR=ON` で **Release** ビルド green ([[feedback_ks_release_build_required]])、tests/vector green。
2. サンプル SVG (バー枠 + アイコン) を体積メッシュ化し collect が DrawItem を返す。
3. 頂点モーフ・extrude 深度アニメが update で反映 (dirty 管理含む)。
4. Pictor TextSvgRenderer のパスを add_path_node して 3D 文字メッシュが出る。
5. module_list 反映、spec 同期。Pictor 3D 描画統合・KS HUD は別 PR (consumer)。

## 9. 実装ステップ (prototyping-flow)
1. svg_parser + path + flatten + tessellator(earcut) で「SVG → 2D 三角形メッシュ」→ テストで面積検証。← 粗く動かす (最初)
2. extruder で体積化 + 法線。
3. VectorScene (ノード/transform/collect) + affine アニメ。
4. 頂点モーフ + extrude 深度アニメ + bind。
5. Pictor TextSvgRenderer 連携 (3D 文字)。
6. tests 整備、Release 受け入れ。Pictor 3D 描画統合は consumer(ui_layout/KS) 側 PR。

## 委託メモ (Codex)
- cwd = `E:/Document/Ars/ergo`。ブランチ `feat/ergo-vector` 新規。Ergo は **feat ブランチ + PR 必須・main 直 push 禁止** (CLAUDE.md)。
- SOLID 全 5 厳守 (Ergo CLAUDE.md): モジュール自己完結・Pictor 非依存・interface 分離 (parser/tessellator/extruder/scene を疎結合)。God Class 禁止。
- earcut.hpp は固定 commit で `third_party/earcut/` に vendoring。MSVC `/utf-8`。
- 不明点 (Pictor パス型との共通化方法、even-odd の earcut 限界、法線の向き規約) は **着手前に Pictor `TextSvgRenderer` 実体を確認**してから決める。
