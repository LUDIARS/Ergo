# render_pipeline plugin — Profile Graph Editor

`tools/ergo` 統合開発者ツールの組み込みプラグイン。 Pictor の **パイプラインプロファイル** (`Pictor/profiles/*.profile.json`) を単一の **NodeGraph** で表示・編集する。

Phase 3 (`feat/render-pipeline-graph-ui`) で旧 Scanner / Timeline / Profile Editor の 3 モードを廃止して **単一のグラフエディタ画面に統合**。 Pictor 系統B 解体 (LUDIARS/Pictor PR #60 系列) で「ハードコードされた描画パス」 が消えるため、 静的 scan は不要になり、 Profile が single source of truth になった。

> 設計詳細: `spec/tool/render_pipeline_system_b.md`。 Pictor 側の正本スキーマ: `Pictor/spec/pipeline-profile-config.md` と `Pictor/spec/pipeline-system-b-config.md` (Phase 3 = schema v2)。

---

## UI 概要

```
[Profile 選択 ▼] [保存] [再読込] ●  [✓ GPU timing]
┌───────────────┬─────────────────────────────────┬───────────────┐
│ Attachments   │  GraphView (vis-network)         │ Inspector     │
│               │                                  │               │
│ + attachment  │   ┌────┐   ┌────┐                │ pass_name     │
│ scene_hdr…    │   │ P1 │──►│ P2 │                │ pass_type ▼   │
│ scene_depth   │   └────┘   └────┘                │ render_targets│
│ swapchain     │                                  │ input_textures│
│               │                                  │ attachment_ops│
└───────────────┴─────────────────────────────────┴───────────────┘
```

- **左 (AttachmentPanel)**: `attachments[]` のリスト。 + ボタンで追加、 × で削除 (参照していた pass の `render_targets` / `input_textures` から自動除去)。 kind (COLOR / DEPTH / SWAPCHAIN_COLOR) を色帯で表示。
- **中央 (GraphView)**: 各ノード = 1 つの `RenderPass`、エッジ = "B の `input_textures` が A の `render_targets` を参照している" 関係。 vis-network の `physics:false / smooth:false` でアニメは無効、 `hierarchical` (LR) を既定。 ノードはドラッグで再配置可能 (永続化なし)。
- **右 (InspectorPanel)**: 選択中の pass の全フィールド (pass_name / pass_type / sort_mode / filter_mask / shader_override) + `render_targets` / `input_textures` のドロップダウン編集 + `attachment_ops` グリッド (load / store / initial_layout / final_layout)。 「Auto-fill from targets」 ボタンで Pictor 側既定推論 (color=CLEAR/STORE/SHADER_READ_ONLY、 depth=CLEAR/DONT_CARE/DSV、 swapchain=CLEAR/STORE/PRESENT_SRC_KHR) を 1 クリック適用。

## 操作

| 操作 | 効果 |
|------|------|
| ヘッダの **+ Pass** | 新規 RenderPass を追加 (pass_type=OPAQUE 既定) |
| ノードを選択 | InspectorPanel に pass プロパティを開く |
| ノードを右クリック → addEdge (ダブルクリック→edit edge mode) | 出力ノード → 入力ノードへエッジを描き、 producer の最初の `render_targets[0]` を consumer の `input_textures[]` に追加 |
| InspectorPanel の `× pass を削除` | pass を消す (他の pass の参照は手動で掃除) |
| AttachmentPanel の × | attachment を消す + 全 pass の `render_targets` / `input_textures` / `attachment_ops` から該当名を除去 |
| Ctrl+S | 保存 (POST /api/profile)。 ファイル名は `<lowercased-profile_name>.profile.json` の規約で再導出 |

## エンドポイント

```
GET  /render_pipeline/api/profiles              → list *.profile.json
GET  /render_pipeline/api/profile/:file         → 1 profile (normalized to schema v2)
POST /render_pipeline/api/profile               → save (body: {profile, file?})
GET  /render_pipeline/api/health                → status
WS   /render_pipeline/ws                        → 双方向 (下記)
```

WS 経路:
- サーバ → 全 UI クライアント: `{op:"profiles-changed", file}` (別クライアントの保存通知)
- クライアント (KS) → サーバ → 全 UI: `{op:"timing", frame, passes:[{id, us}]}` の relay (Phase 2 §6.1 GPU timestamp)

## timing オーバーレイ

WS で `{op:"timing"}` を受けると `state.timing[pass_id] = us` に格納し、 GraphView のノード背景色を「最遅 pass を 100% とした相対比」 で warm-shift する。 ノード位置はアニメせず、 色だけが変わる。 上部の `GPU timing オーバーレイ` チェックボックスでオン/オフ。 pass id は Pictor 側の `pass_name` (= scanner の旧 PASS_DAG 名前) にそのまま一致させる。

## ファイルマップ

```
tools/ergo/src/plugins/render_pipeline/
├── index.ts            # plugin factory (Hono + WS)
├── profile_schema.ts   # schema v2 (TypeScript mirror of pictor::PipelineProfileDef)
├── profile_store.ts    # disk-backed list / load / save
└── ui/
    ├── index.html      # シェル DOM
    ├── styles.css      # 3 ペイン + GraphView スタイル
    └── app.js          # 単一 GraphView 実装
```

旧 `scanner/render_pipeline_scan.py` / `scanner/render_pipeline.json` は Phase 3 で撤去。 plugin 側の `/api/snapshot` / `/api/rescan` も撤去。

## schema v2 (Phase 3)

`PipelineProfileDef` に `attachments: AttachmentDef[]` を追加、 `RenderPassDef` に `attachment_ops: AttachmentOpsDef[]` を追加。 v1 profile (attachments 未宣言) は読み込み時に組み込み既定の 3 件 (`scene_hdr_color` / `scene_depth` / `swapchain`) を補完して開く (Pictor 側 `default_attachments()` と同一テーブル)。 詳細フィールドは `spec/tool/render_pipeline_system_b.md` §3 を参照。

## 既知の制限 (Phase 3 時点)

- multiple subpass (1 VkRenderPass 内の複数 subpass) は schema v2 で未対応 (Phase 4)
- subpass dependency の明示は UI で編集できず、 Pictor 側が `attachment_ops` の `final_layout` ↔ 次 pass の `initial_layout` から自動生成する
- ~~attachment usage ビットの組み合わせ検証なし~~ → **解消** (Phase 4): Inspector に usage 整合警告を表示 (`render_targets` が COLOR/DEPTH 用 usage を持つか、 `input_textures` が SAMPLED を持つか、 未登録参照のチェック)
- ~~ノード位置のドラッグ永続化なし~~ → **解消** (Phase 4): drag end で `profile._editor.nodePositions` に保存、 次回ロードで復元 (Pictor は unknown key を skip するので影響なし)
- 並行編集の競合解決は last-write-wins。 別クライアントの保存通知で未保存差分なし時は静かに再読込
