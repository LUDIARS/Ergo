# render_pipeline plugin — Phase 3 (系統B 編集対応)

設計書 — 2026-05-23 起草。Pictor 側 `spec/pipeline-system-b-config.md` (Phase 3) の Ergo plugin 側対応。Profile Editor を v1 → v2 にアップグレードし、`attachments[]` と各 RenderPass の `attachment_ops[]` を編集できるようにする。

本書は `spec/tool/render_pipeline.md` の追補。本体仕様 (Scanner / Timeline / Profile Editor v1 の振る舞い) はそのまま維持しつつ、Profile Editor の v2 拡張点だけを追記する。

## 1. ゴール

- `*.profile.json` の v2 スキーマ (attachments + attachment_ops) を編集できる
- v1 / v2 を schema version で識別、v1 を開いたら自動で v2 として保存可能 (attachments は既定の 3 件 = `scene_hdr_color` / `scene_depth` / `swapchain` を C++ 既定と同じ値で補完)
- attachment 名は Profile 内全体でオートコンプリート (タイポ抑止)
- `render_targets[]` と `attachment_ops[]` を整合させる UI (片方の編集が自動で他方に追従)

## 2. 非ゴール

- Scanner / Timeline の v2 対応 (Scanner は系統B 側読み取り専用、attachment は scanner snapshot 側で既に表現済み)
- WebGPU-style render graph 構築 (依存推論は Pictor 側、UI は宣言を編集するだけ)
- Pictor との live sync (file watcher は既存 v1 と同じ仕様)

## 3. schema v2 (TS 側)

`tools/ergo/src/plugins/render_pipeline/profile_schema.ts` を更新。

```ts
export type AttachmentKind = "COLOR" | "DEPTH" | "SWAPCHAIN_COLOR";
export type AttachmentSizing = "SWAPCHAIN_RELATIVE" | "ABSOLUTE";

export interface AttachmentDef {
    name: string;
    kind: AttachmentKind;
    format: string;                    // VK_FORMAT_* 文字列 (R16G16B16A16_SFLOAT 等)
    sizing: AttachmentSizing;
    scale?: number;                    // sizing=SWAPCHAIN_RELATIVE
    width?: number;                    // sizing=ABSOLUTE
    height?: number;                   // sizing=ABSOLUTE
    usage: string[];                   // VK_IMAGE_USAGE_* (短縮: COLOR_ATTACHMENT / SAMPLED / DEPTH_STENCIL_ATTACHMENT 等)
    clear_color?: [number, number, number, number];  // kind=COLOR
    clear_depth?: number;                              // kind=DEPTH
}

export type AttachmentLoadOp = "LOAD" | "CLEAR" | "DONT_CARE" | "NONE";
export type AttachmentStoreOp = "STORE" | "DONT_CARE" | "NONE";

export interface AttachmentOps {
    attachment: string;                // attachments[] の name と一致
    load: AttachmentLoadOp;
    store: AttachmentStoreOp;
    initial_layout: string;            // VK_IMAGE_LAYOUT_* 短縮
    final_layout: string;
}

export interface RenderPassDef {
    /* 既存フィールド ... */
    attachment_ops?: AttachmentOps[];  // 省略可。空時は render_targets から既定推論
}

export interface PipelineProfileDef {
    version: 1 | 2;
    /* 既存 ... */
    attachments?: AttachmentDef[];     // v2 で追加。v1 では undefined
    render_passes: RenderPassDef[];
    /* ... */
}
```

予約名 `swapchain` (kind=SWAPCHAIN_COLOR) は常に Profile に存在することを期待 (loader 側で C++ 既定を補完するが、UI 上は明示宣言を推奨)。

## 4. profile_store.ts

- `load(filepath)` で `version` を読み、欠落 (v1) なら 1 として扱う
- `save(filepath, profile)` は常に `version: 2` で書き出す。`attachments` が空なら built-in 既定 3 件を補完して書く (loader 側既定値と完全一致させる)
- v1 → v2 アップグレード時の既定値テーブルは別ファイル `default_attachments.ts` に切り出し (Pictor 側 C++ ハードコード fallback と単一ソースにする — JSON で読み込めるよう静的 JSON を import)

## 5. UI (Profile Editor v2)

### 5.1 トップレベルレイアウト

現状の Profile Editor は左ペイン (プロファイル一覧) + 右ペイン (フィールドフォーム) の 2 ペイン。v2 で右ペインに **タブ** を導入:

| タブ | 内容 |
|------|------|
| **General** (既存) | profile_name / rendering_path / max_lights / msaa_samples / gpu_driven / compute_update / shadow / GI 等のスカラ群 |
| **Attachments** (新規 v2) | `attachments[]` の配列エディタ。各エントリは expandable カード |
| **Render Passes** (既存を拡張) | `render_passes[]` のリスト。各 pass の中に attachment_ops のサブグリッド (v2) |
| **Post-Process** (既存) | post_process[] (Phase 2 で typed) |
| **Memory / GI / Shadow** (既存) | 残りのサブ構造 |

### 5.2 Attachments タブ

```
[+ Add attachment]

┌── scene_hdr_color  [COLOR ▼]                            [×]
│   format:     R16G16B16A16_SFLOAT ▼ (色用フォーマット候補)
│   sizing:     SWAPCHAIN_RELATIVE ▼   scale: [1.00]
│   usage:      ☑ COLOR_ATTACHMENT  ☑ SAMPLED  ☐ TRANSFER_SRC
│   clear_color: R[0.0] G[0.0] B[0.0] A[1.0]
└──

┌── scene_depth  [DEPTH ▼]                                 [×]
│   format:     D32_SFLOAT ▼ (深度用フォーマット候補)
│   sizing:     SWAPCHAIN_RELATIVE ▼   scale: [1.00]
│   usage:      ☑ DEPTH_STENCIL_ATTACHMENT
│   clear_depth: [1.0]
└──

┌── swapchain  [SWAPCHAIN_COLOR ▼ (固定)]                  [×]
│   format:     (swapchain format に従う, 表示のみ)
│   sizing:     SWAPCHAIN_RELATIVE (1.0 固定)
│   usage:      COLOR_ATTACHMENT (固定)
│   clear_color: (ランタイム既定)
└──
```

- kind=SWAPCHAIN_COLOR のエントリは1つだけ許可 (UI 上 dropdown グレーアウト)
- format/usage は kind に応じた候補プリセットを提示 (色用 vs 深度用)
- 名前変更時は Render Passes タブの参照箇所も連動更新 (rename refactor)

### 5.3 Render Passes タブの拡張

既存の各 pass エントリに「Attachment ops」グリッドを追加:

```
RenderPass: "Scene HDR"
  pass_type:     OPAQUE ▼
  shader_override: none
  render_targets:  [scene_hdr_color] [scene_depth]  [+ add]
  attachment_ops:                                              [auto-fill from targets]
  ┌────────────────────┬────────┬─────────┬──────────────┬─────────────────────────┐
  │ attachment         │ load   │ store   │ initial      │ final                   │
  ├────────────────────┼────────┼─────────┼──────────────┼─────────────────────────┤
  │ scene_hdr_color ▼  │ CLEAR ▼│ STORE ▼ │ UNDEFINED ▼ │ SHADER_READ_ONLY_OPT ▼ │
  │ scene_depth ▼      │ CLEAR ▼│ DONTCAR ▼│ UNDEFINED ▼ │ DEPTH_STENCIL_OPT ▼   │
  └────────────────────┴────────┴─────────┴──────────────┴─────────────────────────┘
  input_textures:  [+ add]
```

- `render_targets[]` を編集すると `attachment_ops[]` に同名行が自動追加 (既定値 = CLEAR/STORE) / 不要行は自動削除
- attachment dropdown は Profile 全体の `attachments[]` から候補を提示
- load=CLEAR / store=DONT_CARE の値域検証は警告レベル (依然保存はできる)
- pass_type=POST_PROCESS の場合は attachment_ops 編集を grey out (Post-Process タブ側で別管理)

### 5.4 「Auto-fill from targets」ボタン

`render_targets` を変更後にこのボタンを押すと、`attachment_ops` が次のルールで再生成される:
- COLOR attachment: load=CLEAR, store=STORE, initial=UNDEFINED, final=SHADER_READ_ONLY_OPTIMAL
- DEPTH attachment: load=CLEAR, store=DONT_CARE, initial=UNDEFINED, final=DEPTH_STENCIL_ATTACHMENT_OPTIMAL
- SWAPCHAIN_COLOR: load=CLEAR, store=STORE, initial=UNDEFINED, final=PRESENT_SRC_KHR

これは Pictor 側既定推論 (`spec/pipeline-system-b-config.md` §3.2 末尾) と同一テーブルを使う (`default_attachment_ops.ts` に切り出し)。

## 6. API 拡張

既存 `POST /api/profile` (`profile_store.ts`) はそのまま使う。クライアントが v2 形式の JSON を送れば書き出す。

新規 endpoint なし。

## 7. ヒント表示の更新

Profile Editor 内のフィールドヒントを更新:
- `render_targets`: 「v2 では `attachment_ops` と整合させてください。空時は既定 (CLEAR/STORE) で推論されます。」
- `input_textures`: 「v2 から Pictor の `RenderPassScheduler::execute()` が descriptor set 0 の binding 0..N に bind します (固定 layout)」
- `shader_override`: 「v2 から `ShaderRegistry` から resolve されて `vkCmdBindPipeline` されます (`handle:<u32>` 形式)」

既知の制限セクションの「系統B 未配線」 一文は v2 で削除 (Phase 3 で配線するため)。

## 8. テスト

- `tools/ergo/src/plugins/render_pipeline/__tests__/profile_schema_v2.test.ts` (新規): v1 round-trip / v2 round-trip / v1→v2 アップグレードで既定 attachments が補完される / rename refactor が render_passes[] の参照を更新する
- Existing tsc build に乗せて npm run build EXIT 0 確認

## 9. 既知の制限 (v2 時点)

- `attachments[]` の依存解析 (DAG 自動構築) は scanner 側の役割なので Editor は素直な配列編集のみ
- multiple subpass (1 つの VkRenderPass 内の複数 subpass) は v2 でも未対応 — Phase 4 で `subpasses[]` を `RenderPassDef` に追加する
- `dependencies[]` (subpass dependency の明示) は v2 では UI で編集できず、Pictor 側が attachment_ops の `final_layout` ↔ 次 pass の `initial_layout` から自動生成する。違反すると validation layer エラー
- attachment usage ビットの組み合わせ検証なし (例: SAMPLED 無しでも他 pass が input_textures として参照すれば実行時エラー)

## 10. 実装手順

1. `default_attachments.ts` / `default_attachment_ops.ts` を作成 (Pictor 側既定と完全一致)
2. `profile_schema.ts` に AttachmentDef / AttachmentOps の型と zod パーサを追加
3. `profile_store.ts` の load を v1/v2 両対応、save は常に v2
4. Profile Editor UI の右ペインに **Tab** コンポーネントを導入し既存フォームを General タブに収める
5. Attachments タブ実装 (配列エディタ + 名前 rename refactor)
6. Render Passes タブに attachment_ops グリッド + Auto-fill ボタン
7. ヒント文言の更新 + 既知の制限の更新
8. tsc / npm run build EXIT 0 確認
9. PR
