# render_pipeline plugin — Render Pipeline Visualizer

## 概要

Ergo 統合開発者ツールの組み込みプラグイン。 Pictor のレンダリングパス
構成 (render pass DAG / pipeline spec / shader / attachment flow) を
Web 上で可視化する。 大量のシェーダと多段ポストプロセスを抱えるレンダラの
全体像を 1 画面で俯瞰 + 詳細ドリルできるのが狙い。

データ源はハイブリッド構成:

- **静的 scan**: `scanner/render_pipeline_scan.py` (Python) が Pictor の
  ソースを走査し、 pipeline 生成箇所から blend/cull/depth/topology +
  使用シェーダを抜き出して JSON 化。 同時に Pictor/ergo の全シェーダを
  source 込みで snapshot に embed
- **render pass DAG**: pass 同士の依存関係は scanner では追えないので、
  `scanner/render_pipeline_scan.py` 内に Pictor の設計 (scene_hdr →
  decal_compose → postprocess → hud_load) を人手宣言。 attachments の
  produces/consumes を辿ると DAG が自動で組まれる
- **将来 (Phase 2)**: 実行時 GPU timestamp を Pictor が WS で
  `{op:"timing", pass:..., us:...}` として publish → 各ノードに重ねる

## 場所

- 実装: `tools/ergo/src/plugins/render_pipeline/`
  - `index.ts` — Hono routes + WS (Phase 2 用 hub)
  - `ui/index.html` — DAG + tabs (Pipelines / Shaders / Attachments)
  - `ui/styles.css`
  - `ui/app.js` — vis.js Network + highlight.js
- Scanner: `tools/ergo/scanner/render_pipeline_scan.py`
- Snapshot: `tools/ergo/scanner/render_pipeline.json` (生成物、 commit する)
- 登録: `tools/ergo/src/core/registry.ts` の `PLUGIN_FACTORIES`

## URL

| URL                                                 | 役割                                |
|-----------------------------------------------------|------------------------------------|
| `http://localhost:5170/render_pipeline/`            | UI                                  |
| `http://localhost:5170/render_pipeline/api/snapshot`| 最新スナップショット JSON           |
| `http://localhost:5170/render_pipeline/api/health`  | スナップショットの有無 + サマリ統計 |
| `http://localhost:5170/render_pipeline/api/rescan`  | scanner を子プロセス起動            |
| `ws://localhost:5170/render_pipeline/ws`            | Phase 2 timing relay (現在は ack)    |

## 表示要素

### Pass DAG (左ペイン)

vis.js Network で hierarchical layout。 ノードは pass、 エッジは attachment
の引き渡し (ラベル付き)。 ノードをクリックすると下に詳細 (consumes /
produces / draws / description) が出る。

### Pipelines (右ペイン Pipelines タブ)

scanner が抜き出した pipeline 一覧を表で表示。 列:
- 関数 + source file:line
- blend (on/off + op + factor)
- cull mode
- depth (test / write tag + compare op)
- topology
- shaders (使用 .vert/.frag)

filter 入力で関数名 / シェーダ名で絞り込み。

### Shaders (右ペイン Shaders タブ)

左: シェーダ一覧 (rel_path + stage + 行数)、 filter 付き
右: 選択シェーダの source (highlight.js GLSL) と layout 抜粋
   (in / out / uniform / buffer の `location=N / type / name` 一覧)

### Attachments (右ペイン Attachments タブ)

attachment (hdr_color / scene_depth / swapchain) の format / usage /
owner / 用途を表で表示。 producer / consumer は DAG 側を見る。

## 静的サイト化 (将来)

`api/snapshot` が無くても `./snapshot.json` を直接 fetch する fallback が
ある。 `scanner/render_pipeline.json` を `dist/snapshot.json` にコピーすれば
GitHub Pages 等にそのまま展開可能。

## 使い方

```sh
# 1) スキャン (Pictor / ergo の現状を JSON 化)
cd tools/ergo/scanner
python render_pipeline_scan.py
#   passes=4 attachments=3 pipelines=6 shaders=66 → render_pipeline.json

# 2) ツール起動
cd ../
npm run dev
# → http://localhost:5170/render_pipeline/

# 3) Web UI 上でも ↻ Rescan ボタンで scanner を起動できる
```

## Phase 2 (実行時 GPU timing) 設計メモ

Pictor 側に VkQueryPool + timestamp を入れて、 各 pass の end_render_pass
で `vkCmdWriteTimestamp` を呼ぶ。 1 frame ぶんの timestamp 配列を読み戻して
`{op:"timing", frame:N, passes:[{id, us}]}` を WS で publish する。 ergo
プラグインは receive → vis.js ノードラベルに "scene_hdr (1.23 ms)" を
追加する。 client は時系列グラフも欲しいので別タブ追加かも。

## 既知の制限

- scanner の C++ 解析は正規表現ベースなので、 関数境界の判定が雑。
  ネストした lambda / 複数 vkCreateGraphicsPipelines を含む関数では
  最後の値しか拾えない場合がある (TODO: tree-sitter 化)
- compute pipeline (vkCreateComputePipelines) は今は scan していない
- ergo 側の C++ pipeline (gpu_particle 等) も今は scan 対象外
- DAG は scanner.py の `PASS_DAG` を編集して手動更新する必要がある
- shader の `version` 表記が `450 core` 等の場合の語尾はそのまま表示
