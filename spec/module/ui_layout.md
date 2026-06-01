# ergo_ui_layout — データ駆動 UI レイアウト基盤 + UI エディタ (設計書)

> 状態: 設計 (2026-06-01)。実装は Codex 委託予定。
> 区分: 新規 Ergo モジュール `ergo_ui_layout` + tools/ergo UI エディタプラグイン。
> 関連: [vector.md](./vector.md) (vector ノードのバックエンド), [ui.md](./ui.md) / ergo_ui_kit (既存プリミティブ描画), [custos.md](./custos.md) (ライブ調整トランスポート), KuzuSurvivors HUD (最初の consumer)。

## 0. 背景・目的

ゲーム UI (HUD / メニュー / レベルアップ等) を **コードに直書きせず、外部データ (JSON) として定義**し、**Figma 的なツールで配置・編集**でき、**ゲーム実行中にもライブ調整**できる汎用基盤を作る。配置データは外部に出し、ロード/書き戻しする。

「Pictor/Ergo 上で」= 特定ゲーム非依存の **Ergo モジュール**として作り、全ゲームで再利用 (KS は最初の consumer)。

### スコープ
- **やる**: UI レイアウトの宣言的データモデル (JSON) + ランタイム (ロード/構築/レイアウト解決/描画委譲/データバインド/ヒットテスト) + tools/ergo の Figma 的 UI エディタプラグイン + ergo_custos 経由のライブ往復編集。
- **やらない**: 新規描画ラスタライザ (描画は Pictor UIRenderer + ergo_vector に委譲)、複雑なアニメーションタイムライン編集 (ノードのトゥイーン/状態遷移は最小限。本格アニメは vector ノード=Lottie 側)。

## 1. アーキテクチャ

```
[*.uilayout.json] ──load──> ergo_ui_layout ランタイム
                                │  ・ノードツリー構築
                                │  ・レイアウト解決 (anchor/constraint/flex)
                                │  ・data-binding (bind式 → ランタイム値)
                                ├─ プリミティブノード → Pictor UIRenderer (rect/9slice/image/text/clip)
                                ├─ text ノード        → Pictor TextSvgRenderer/TextImageRenderer (ベクター文字。3D 押し出しは ergo_vector 経由も可)
                                └─ vector ノード       → ergo_vector (SVG → ポリゴン化 → extrude 体積 3D メッシュ) → Pictor 3D 描画
        ▲                                   │
        │ read/write JSON                   │ live patch (JSON-merge)
   tools/ergo UI エディタ ◀── ergo_custos bridge (HTTP/WS) ──▶ 実行中ゲーム
```

- **描画は再実装しない**。プリミティブは ergo_ui_kit + Pictor UIRenderer、文字は Pictor のベクターテキスト経路、ベクターアセットは ergo_vector。ergo_ui_layout は「データ→これらへの描画コマンド生成 + レイアウト計算 + バインド」を担う中間層。
- KS の現状 `hud_layer.cpp` は最終的に「uilayout を 1 枚ロードして描画する」薄い consumer に置き換わる (KS 側別 PR)。

## 2. モジュール構成 (Ergo 規約準拠)

| 項目 | 値 |
|---|---|
| name | `ergo_ui_layout` |
| header_dir | `include/ergo/ui_layout/` |
| source_dir | `src/ui_layout/` |
| test_dir | `tests/ui_layout/` |
| spec | `spec/module/ui_layout.md` (本書) |
| cmake_option | `ERGO_BUILD_UI_LAYOUT` |
| tool_plugin | `ui_layout` (tools/ergo プラグイン id) |
| status | preview |

`module_list.yaml`/`.md` 追記。SRP でファイル分割 (model / parser / layout solver / binding / renderer-adapter / hittest を別ファイル)。God Class 禁止。

## 3. データモデル (*.uilayout.json)

宣言的・前方互換 (`schema_version`)。座標は基準解像度 (`design_size`) 内の論理px、実画面へはスケール。

```jsonc
{
  "schema_version": 1,
  "name": "kuzu_hud",
  "design_size": { "w": 1280, "h": 720 },
  "root": {
    "id": "root",
    "type": "container",          // container | rect | nine_slice | image | text | vector
    "layout": "absolute",         // absolute | row | column (flex)
    "rect": { "x": 0, "y": 0, "w": 1280, "h": 720 },
    "anchor": { "h": "left", "v": "top" },   // 親基準アンカー (Figma constraint)
    "stretch": { "left": null, "right": null, "top": null, "bottom": null }, // 端固定px (constraint)
    "children": [
      {
        "id": "hp_bar_frame",
        "type": "vector",
        "rect": { "x": 24, "y": 56, "w": 320, "h": 28 },
        "vector": { "src": "data/hud/hp_bar.svg", "fit": "stretch", "extrude": 6.0 },
        "binds": [
          { "target": "node:hp_fill", "op": "scale_x", "expr": "hp_ratio" },
          { "target": "node:hp_fill", "op": "color", "expr": "hp_low ? '#c0392b' : '#2ecc71'" }
        ],
        "anim": { "on": "hp_low", "morph": "pulse", "extrude_pulse": 2.0 }  // vector(頂点モーフ/extrude深度) のトリガ
      },
      {
        "id": "timer",
        "type": "text",
        "rect": { "x": 540, "y": 16, "w": 200, "h": 32 },
        "text": { "font": "data/fonts/main.ttf", "size": 28, "align": "center", "color": "#ffffff" },
        "binds": [ { "target": "self", "op": "text", "expr": "fmt_mmss(time_left)" } ]
      }
    ]
  }
}
```

ノード型: `container` (レイアウトのみ) / `rect` / `nine_slice` (9-slice 画像) / `image` (テクスチャ) / `text` (ベクター文字) / `vector` (SVG/Lottie via ergo_vector)。

### data-binding
- `binds[]`: `target` (self / node:<svg内id> / 属性), `op` (text / scale_x / fill_color / opacity / visible / transform), `expr` (バインド式)。
- **バインド式**は最小の安全な評価器: 変数参照 + 三項 + 比較 + 少数の関数 (`fmt_mmss`, `clamp` 等)。チューリング完全にしない。変数は consumer がフレーム毎に供給する `BindContext` (`map<string, Value>`: number/bool/string)。
- KS HUD のバインド変数例: `hp_ratio, xp_ratio, gauge_ratio, time_left, kill_count, level, hp_low, slot_a_label, ...`。

### レイアウト解決
- `absolute` (rect 直接) + `anchor`/`stretch` (Figma constraint: 親リサイズ時の追従) + `row`/`column` flex (gap/justify/align) の最小セット。実画面解像度へは design_size 基準でスケール (letterbox or scale)。

## 4. ランタイム API (include/ergo/ui_layout/)

```cpp
namespace ergo::ui_layout {

class Document {
public:
  static std::unique_ptr<Document> load_file(const std::string& path);
  static std::unique_ptr<Document> load_json(std::string_view json);
  bool save_file(const std::string& path) const;          // エディタ/ライブ保存用 (ラウンドトリップ)

  void set_viewport(int w, int h);                         // 実解像度
  void update(const BindContext& ctx, float dt);           // バインド適用 + vector advance + レイアウト解決
  void emit(RenderAdapter& adapter);                        // 描画コマンド発行 (Pictor 非依存 IF)

  // エディタ/ライブ編集用
  Node* find(std::string_view id);
  void  apply_patch(std::string_view json_merge_patch);    // ライブ差し替え (custos 経由)
  std::string to_json() const;

  // ヒットテスト (将来のインタラクティブ UI / エディタ選択)
  Node* hit_test(float x, float y);
};

// 描画バックエンド抽象 (ergo_ui_layout は Pictor を直接知らない = レイヤ単方向)
struct RenderAdapter {
  virtual void draw_rect(...) = 0;
  virtual void draw_nine_slice(...) = 0;
  virtual void draw_image(TextureRef, ...) = 0;
  virtual void draw_text(std::string_view, const TextStyle&, ...) = 0;   // Pictor ベクターテキストに委譲
  virtual void draw_vector_mesh(const VectorDrawItem&) = 0;              // ergo_vector の 3D メッシュを Pictor 3D 描画へ
  virtual void push_clip(...) = 0; virtual void pop_clip() = 0;
};

} // namespace
```

- consumer (KS) が Pictor 実装の `RenderAdapter` を提供 (UIRenderer + TextSvgRenderer + ergo_vector を束ねる)。ergo_ui_layout 本体は Pictor 非依存に保つ。

## 5. UI エディタ (tools/ergo プラグイン `ui_layout`)

- tools/ergo (Electron + プラグインホスト、既存) に **UI レイアウトエディタプラグイン**を追加。[[feedback_ergo_editor_plugin_pack]] の一般則どおり tools/ergo プラグインとして実装 (KS 専用 fork はしない)。
- 機能 (Figma 的):
  - キャンバスに uilayout を描画 (Web 側は SVG/Canvas で同等プレビュー、真実はゲーム側描画)、ノード選択/移動/リサイズ/階層編集。
  - プロパティパネル: rect/anchor/stretch/flex、ノード型別プロパティ、binds 編集。
  - `*.uilayout.json` の open / save (外部データが正)。
  - vector ノードは参照 SVG/Lottie のサムネ表示。
- **ライブ往復** (ergo_custos): エディタ ↔ 実行中ゲームを ergo_custos ブリッジ (HTTP/WS、KS に `ergo_custos_bridge.json` 既設) で接続。エディタでの変更を `apply_patch` で実機に即反映、ゲーム側の手動調整も pull、保存で JSON 書き戻し。variable プラグイン ([[project_variable_editor]] = ergo_bind) と同じライブチューニング思想。
- ライブ調整の伝送単位は **JSON Merge Patch** (ノード単位の差分) を既定。

## 6. KS HUD consumer (別 PR / KS 側)
- `data/hud/kuzu_hud.uilayout.json` を作成 (spec/hud.md のレイアウトを移植: timer/kill/hp/xp/gauge/slot/toast)。
- `src/render/layers/hud_layer.cpp` を「Document をロードし、`hud.sync()` の値を BindContext に詰めて update→emit」する薄い consumer に置換。BitmapText 依存を除去。
- vector ノード (hp_bar.svg 等) は ergo_vector、文字は Pictor ベクターテキスト。
- これにより KS HUD は「データ編集 = エディタ / ライブ調整」で回る = goal2 の HUD 仕上げ項目を満たす。

## 7. テスト (tests/ui_layout/)
- JSON ロード→ツリー構築→find(id)。
- レイアウト解決: anchor/stretch/flex の代表ケースで rect が期待値。
- バインド式評価: 変数/三項/比較/fmt_mmss、未知変数は安全にデフォルト。
- apply_patch でノード属性が変わり to_json でラウンドトリップ一致。
- RenderAdapter をモックして emit が期待コマンド列を出す (Vulkan 不要)。

## 8. 受け入れ基準
1. `ERGO_BUILD_UI_LAYOUT=ON` で Release ビルド green、tests/ui_layout green (Vulkan 非依存)。
2. サンプル uilayout を load→update(bind)→emit(モック) が通る。
3. tools/ergo に `ui_layout` プラグインが立ち上がり、JSON の open/save/ノード編集ができる。
4. ergo_custos 経由のライブ patch 往復が動く (最小: 1 ノードの rect/色をエディタ→実機反映)。
5. module_list 反映、spec 同期。KS HUD 置換は別 PR。

## 9. 実装ステップ (prototyping-flow)
1. データモデル + parser + レイアウト解決 (absolute+anchor) + RenderAdapter IF + モックテスト。← まず「粗く動く」
2. プリミティブ (rect/nine_slice/image/text) を Pictor RenderAdapter で実描画 (KS 側最小consumer で1枚出す)。
3. vector ノード (ergo_vector 連携) + binds。
4. tools/ergo `ui_layout` プラグイン (編集 + save)。
5. ergo_custos ライブ往復。
6. flex/応用レイアウト、KS HUD 全面移植 (別 PR)。

## 委託メモ (Codex)
- cwd = `E:/Document/Ars/ergo`。ブランチ `feat/ergo-ui-layout` 新規。Ergo は feat ブランチ + PR 必須 ([[feedback_ergo_branch_pr_required]])。
- ergo_vector ([vector.md](./vector.md)) に依存 (vector ノード)。ergo_vector が先 or 並行。vector 未完なら vector ノードはスタブで進め、IF を確定。
- SRP / ファイル分割 / レイヤ単方向 (ergo_ui_layout は Pictor 非依存、RenderAdapter で抽象)。既存 ergo_ui_kit / src/custos / Pictor UIRenderer・TextSvgRenderer の実体を着手前に確認し、再実装せず再利用する。
