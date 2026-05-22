# render_pipeline plugin — Phase 3 (系統B 解体・単一グラフエディタ化)

設計書 — 2026-05-23 起草。Pictor 側 `spec/pipeline-system-b-config.md` (Phase 3) の Ergo plugin 側対応。

Phase 3 で Pictor 系統B (実 `VkRenderPass` チェーン) を解体し JSON 駆動にする。**ハードコード描画パスは存在しなくなる** ので、Phase 2 まであった Scanner モード (Pictor C++ ソースを Python regex でスキャン) は **撤去**。`*.profile.json` が **唯一の真理** になる。

UI は Profile Editor / DAG / Timeline の 3 モード分割をやめ、**単一のノードグラフエディタ画面**に統合する。グラフ上で pass ノードを drag-to-connect し、 同じ画面で per-pass プロパティを編集し、 同じ画面に GPU timing をオーバーレイ表示する。

## 1. ゴール

- Scanner 関連 (Python parser + scanner snapshot + Scanner モード UI) を完全削除
- Profile Editor / DAG / Timeline を 1 つの「グラフエディタ」へ統合
- ノードを drag で接続 (出力 attachment → 次 pass の入力 attachment) してパイプラインを構築
- 編集中も Phase 2 §6.1 の timing オーバーレイを同グラフで見られる
- アニメーション無効 (vis.js / Cytoscape どちらを使うにせよ `physics: false` / `smooth: false`)
- v2 スキーマ (`attachments[]` + `attachment_ops[]`) を素直に読み書き

## 2. 非ゴール

- WebGPU-style render graph 自動推論 (依存推論は Pictor 側 `PipelineCompiler` の仕事)
- 立体 / 3D ビュー (純 2D グラフのみ)
- live 共同編集 (last-write-wins、別クライアントの保存通知で再読込のみ — Phase 2 と同じ)
- node graph の任意レイアウト保存 (Phase 3 では auto-layout のみ。 ユーザ手動配置の永続化は Phase 4)

## 3. データ源 (Phase 2 までとの違い)

| Phase 2 まで | Phase 3 |
|---|---|
| Scanner snapshot (`scanner/render_pipeline.json`) ← Python が C++ から regex 抽出 | **撤去** |
| `Pictor/profiles/*.profile.json` ← Profile Editor が編集 | **唯一の真理**。 グラフエディタが直接編集 |
| DAG レイアウト = scanner snapshot の `PASS_DAG` 手動配列 | profile の `render_passes[]` + `attachments[]` から自動構築 |
| Timeline バー = scanner の `passes[]` + topological level | profile の `render_passes[]` + GPU timing relay |

scanner snapshot を見ていた API (`GET /render_pipeline/api/snapshot`、 `POST /render_pipeline/api/rescan`) は撤去。 代わりに `GET /render_pipeline/api/profile/:name` (既存) と `POST /render_pipeline/api/profile` (既存) だけが残る。

## 4. 削除するもの

- `tools/ergo/scanner/render_pipeline_scan.py`
- `tools/ergo/scanner/render_pipeline.json`
- plugin 側 `index.ts` の scanner サブプロセス起動コード + `/api/snapshot` / `/api/rescan` ルート
- plugin 側 UI の Scanner モード (`app.js` 内 mode=`scanner` 分岐 + 関連 DOM)
- plugin 側 UI の Timeline モード (新グラフへ統合、後述)
- 既存 `spec/tool/render_pipeline.md` の「モード A: Scanner」「モード T: Timeline」 セクションは Phase 3 PR で大幅短縮 (Profile Editor v1 表記も同様に更新)

## 5. データモデル (Phase 3 グラフ図形)

```
GraphView
  ├─ NodeGraph (vis-network / cytoscape — physics:false, smooth:false)
  │   ├─ node = RenderPass entry
  │   │   ├─ inputs  : input_textures[]  (左端子)
  │   │   ├─ outputs : render_targets[]  (右端子、 attachment_ops の op バッジ付き)
  │   │   └─ overlay : last GPU timing (Phase 2 §6.1) を色 + 数値
  │   └─ edge = 「attachment A は pass X の出力で pass Y の入力」 を表す
  │       (attachment_name でラベル付け、 暗黙の依存は灰色破線)
  ├─ AttachmentPanel (左サイドバー)
  │   └─ attachments[] のフラットリスト + + ボタン (kind / format / sizing 編集)
  └─ InspectorPanel (右サイドバー)
      └─ 選択中 pass の General / Attachment ops / Filter & Sort / Shader override
```

Profile 切替・保存は上部ツールバー (現状の Profile Editor と同等)。

## 6. 操作

| 操作 | 効果 |
|------|------|
| 空白でダブルクリック | 新規 pass ノード追加 (pass_type は OPAQUE 既定) |
| 出力端子 → 入力端子へ drag | エッジを引き、 対応する attachment 名を `render_targets`/`input_textures` に追加 (未宣言 attachment なら sidebar に追加促し) |
| エッジ右クリック → delete | 入力側 pass の `input_textures` から該当 attachment を除去 (出力側はそのまま) |
| ノード選択 | InspectorPanel に該当 RenderPass の全フィールド (General / attachment_ops / filter_mask / shader_override) |
| ノードドラッグ | 表示位置のみ更新 (永続化なし)。 layout dirty フラグは立てるが保存しない |
| AttachmentPanel で新規 attachment | `attachments[]` に追加。 編集中の名前は graph 内エッジラベルへ即時反映 (rename refactor) |
| AttachmentPanel で削除 | `attachments[]` から除去 + 参照していた `render_targets`/`input_textures` から自動除去 (確認ダイアログ) |
| Save (Ctrl+S) | `POST /api/profile` を v2 形式で投げる |
| 別クライアントの保存通知 (`{op:"profile_updated"}`) | 未保存差分なし時のみ静かに再読込、 差分あるときは UI に notify |

## 7. レイアウト

vis-network の `hierarchical` レイアウト (LR) を既定。 attachment の produces/consumes から topological levels を計算し、 同 level の pass は縦方向に並べる。 ユーザがドラッグしたら静的位置を保つ (再計算しない) が、 永続化しない (`physics: false` で auto-shake 抑止)。

アニメーション設定 (vis-network options):
```ts
{
  physics: { enabled: false },
  edges:   { smooth: false },
  layout:  { hierarchical: { enabled: true, direction: "LR", sortMethod: "directed" } },
  interaction: { dragNodes: true, dragView: true, zoomView: true }
}
```

DAG の更新は profile 変更時に diff (追加/削除/属性変更) を vis-network に渡すだけ — full rerender はしない。

## 8. timing オーバーレイ

Phase 2 §6.1 の WS broadcast (`{op:"timing", frame, passes:[{id, us}]}`) は既に Ergo 側で relay 済 (#30)。 GraphView では:
- 各ノード右上に最新 frame の値 (`1.23 ms`) をテキストで重ねる
- ノード背景色を最遅 pass を 100% とした相対比でグラデ着色 (赤=最遅 / 緑=最速)
- `op:"timing"` を受けても**ノードは動かない** (色だけ変わる)

旧 Timeline モードの「実行順ガントチャート」 を残したい場合は、 GraphView 上部に折り畳み可能なバー (1 行分) を置く。 これは閉じた状態が既定 (アニメ無し / scroll 連動なし)。

## 9. schema v2 (TS 側)

`tools/ergo/src/plugins/render_pipeline/profile_schema.ts` を更新:

```ts
export type AttachmentKind = "COLOR" | "DEPTH" | "SWAPCHAIN_COLOR";
export type AttachmentSizing = "SWAPCHAIN_RELATIVE" | "ABSOLUTE";

export interface AttachmentDef {
    name: string;
    kind: AttachmentKind;
    format: string;
    sizing: AttachmentSizing;
    scale?: number;
    width?: number;
    height?: number;
    usage: string[];
    clear_color?: [number, number, number, number];
    clear_depth?: number;
}

export type AttachmentLoadOp = "LOAD" | "CLEAR" | "DONT_CARE" | "NONE";
export type AttachmentStoreOp = "STORE" | "DONT_CARE" | "NONE";

export interface AttachmentOps {
    attachment: string;
    load: AttachmentLoadOp;
    store: AttachmentStoreOp;
    initial_layout: string;
    final_layout: string;
}

export interface RenderPassDef {
    pass_name: string;
    pass_type: "OPAQUE" | "TRANSPARENT" | "DEPTH_ONLY" | "SHADOW"
              | "POST_PROCESS" | "COMPUTE" | "CUSTOM";
    shader_override: string;
    render_targets: string[];
    input_textures: string[];
    attachment_ops?: AttachmentOps[];
    sort_mode: "FRONT_TO_BACK" | "BACK_TO_FRONT" | "NONE";
    filter_mask: number;
    gpu_driven_pass: boolean;
    required_streams: string[];
}

export interface PipelineProfileDef {
    version: 1 | 2;
    profile_name: string;
    rendering_path: string;
    max_lights: number;
    msaa_samples: number;
    gpu_driven_enabled: boolean;
    compute_update_enabled: boolean;
    attachments?: AttachmentDef[];      // v2 で追加
    render_passes: RenderPassDef[];
    post_process: PostProcessDef[];
    /* ... 他既存フィールド ... */
}
```

`default_attachments.ts` / `default_attachment_ops.ts` を作成し、 Pictor 側 C++ ハードコード fallback と完全同期の既定値テーブルを 1 箇所に集中させる (v1 → v2 アップグレード時に補完)。

## 10. profile_store.ts の API

既存:
- `GET /api/profile?name=<n>` — 1 プロファイル取得
- `GET /api/profiles` — リスト
- `POST /api/profile` — 保存 (本文 = profile JSON)

Phase 3 で追加するもの:
- `GET /api/profile/:name/graph` — グラフ用に正規化したノード+エッジリストを返す (server-side で profile から計算)
- `POST /api/profile/:name/graph` — グラフ編集の結果 (ノード追加/削除/エッジ追加/削除) を atomic に適用 → profile JSON へ畳み込んで保存

撤去するもの:
- `GET /api/snapshot` (scanner)
- `POST /api/rescan` (scanner)
- `GET /api/health` の `snapshot_present` フィールド (scanner 撤去で常に false → 削除)

## 11. ファイル / コード変更まとめ

### 11.1 削除

- `tools/ergo/scanner/render_pipeline_scan.py`
- `tools/ergo/scanner/render_pipeline.json`
- `tools/ergo/src/plugins/render_pipeline/index.ts` から scanner プロセス起動 + `/api/snapshot` / `/api/rescan` ルート
- `tools/ergo/src/plugins/render_pipeline/ui/app.js` から Scanner / Timeline モード分岐 + 関連 DOM
- `tools/ergo/src/plugins/render_pipeline/ui/index.html` から Scanner / Timeline モード切替ボタン

### 11.2 新規

- `tools/ergo/src/plugins/render_pipeline/graph_layout.ts` — profile → graph 構造 (nodes + edges) 変換
- `tools/ergo/src/plugins/render_pipeline/default_attachments.ts` — v1→v2 既定値テーブル
- `tools/ergo/src/plugins/render_pipeline/ui/graph_view.js` — vis-network 初期化 + drag-to-connect + inspector 連携
- `tools/ergo/src/plugins/render_pipeline/ui/inspector_panel.js` — 右サイドバー (選択 pass の編集 UI)
- `tools/ergo/src/plugins/render_pipeline/ui/attachment_panel.js` — 左サイドバー (attachments[] 編集)
- `tools/ergo/src/plugins/render_pipeline/__tests__/profile_schema_v2.test.ts` — v1→v2 round-trip / rename refactor

### 11.3 既存改修

- `tools/ergo/src/plugins/render_pipeline/profile_schema.ts` — v2 型 + zod パーサ
- `tools/ergo/src/plugins/render_pipeline/profile_store.ts` — load(v1|v2) / save(v2) / `/graph` endpoint
- `tools/ergo/src/plugins/render_pipeline/index.ts` — scanner 起動撤去 + `/graph` endpoint 配線
- `tools/ergo/src/plugins/render_pipeline/ui/index.html` — モード切替を廃止し単一 GraphView へ
- `tools/ergo/src/plugins/render_pipeline/ui/app.js` — GraphView entry point + WS timing overlay
- `spec/tool/render_pipeline.md` — Phase 3 反映 (Scanner / Timeline モード記述削除、 単一 GraphView の説明に書き換え)

## 12. テスト

- `profile_schema_v2.test.ts`: v1 ↔ v2 round-trip、 既定 attachments 補完、 attachment rename がエッジに伝播、 attachment 削除が render_passes[] の参照を除去
- `graph_layout.test.ts`: profile → nodes/edges 変換、 暗黙依存の灰色化、 attachment 名 collision の検知
- tsc / npm run build EXIT 0

## 13. 既知の制限 (Phase 3 時点)

- multiple subpass (1 つの VkRenderPass 内の複数 subpass) は v2 でも未対応 — Phase 4
- subpass dependency の明示は UI で編集できず Pictor 側 `attachment_ops` ↔ 次 pass `initial_layout` 自動生成に委譲
- attachment usage ビットの組み合わせ検証なし (SAMPLED 無しでも他 pass が input_textures として参照可能 — 実行時 validation 任せ)
- ノードの自動レイアウトは hierarchical (LR) のみ。 spring / radial 等はサポートしない
- ノードの手動位置は永続化しない (グラフを開くたびに自動レイアウト)。 Phase 4 でローカル設定として位置を保持予定

## 14. 実装手順

1. scanner 関連ファイル / コードを削除 (commit 1)
2. profile_schema.ts を v2 化、 default テーブル新設 (commit 2)
3. profile_store.ts に v2 対応 + `/graph` endpoint (commit 3)
4. UI を単一 GraphView へ刷新 (vis-network 初期化、 drag-to-connect、 inspector / attachment panel、 timing overlay) (commit 4)
5. `spec/tool/render_pipeline.md` を Phase 3 化 (commit 5)
6. テスト + tsc / npm run build EXIT 0 確認
7. PR
