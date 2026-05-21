# render_pipeline plugin — Render Pipeline Visualizer + Profile Editor

## 概要

Ergo 統合開発者ツールの組み込みプラグイン。 **2 つのモード**を持つ:

| モード | 何 | 種別 |
|--------|----|------|
| **Scanner** (現状ビュー) | Pictor のハードコードされた Vulkan 実装 (系統B) を静的スキャンした render pass DAG / pipeline / shader / attachment | **読み取り専用** |
| **Profile Editor** (編集ビュー) | `Pictor/profiles/*.profile.json` (系統A の `PipelineProfileDef`) の編集 | **読み書き + ディスク永続化** |

2 つは「Pictor が今どう描いているか」(Scanner) と「Pictor にどう描かせたいか」
(Editor) という別物であり、 **API 名前空間も対象ファイルも完全に分離**している。
UI はヘッダのモード切替ボタンで 2 つの `<main>` を排他表示する。

> 系統A / 系統B の区別は Pictor 側正本 `Pictor/spec/pipeline-profile-config.md` §1 を参照。
> `PipelineProfileDef` (系統A) は宣言的プロファイルデータ、 実 `VkRenderPass` チェーン
> (系統B) はハードコード。 Editor は系統A を編集し、 Scanner は系統B を可視化する。

---

## モード A: Scanner (現状ビュー・読み取り専用)

データ源はハイブリッド構成:

- **静的 scan**: `scanner/render_pipeline_scan.py` (Python) が Pictor の
  ソースを走査し、 pipeline 生成箇所から blend/cull/depth/topology +
  使用シェーダを抜き出して JSON 化。 同時に Pictor/ergo の全シェーダを
  source 込みで snapshot に embed
- **render pass DAG**: pass 同士の依存関係は scanner では追えないので、
  `scanner/render_pipeline_scan.py` 内に Pictor の設計を人手宣言。
  attachments の produces/consumes を辿ると DAG が自動で組まれる
- **将来 (Phase 2)**: 実行時 GPU timestamp を Pictor が WS で
  `{op:"timing", pass:..., us:...}` として publish → 各ノードに重ねる

### 表示要素

- **Pass DAG** — vis.js Network。 ノードは pass、 エッジは attachment 引き渡し。
  ノードクリックで詳細 (consumes / produces / draws / description)
- **Pipelines タブ** — scanner が抜き出した pipeline 一覧表 (blend / cull /
  depth / topology / shaders)、 filter 付き
- **Shaders タブ** — シェーダ一覧 + 選択シェーダの source (highlight.js GLSL)
  と layout 抜粋
- **Attachments タブ** — attachment の format / usage / owner / 用途

このモードは「Pictor の現コードが実際どう描いているか」を映すだけで、
**一切編集できない**。 `↻ Rescan` ボタンで scanner を再実行できる。

---

## モード B: Profile Editor (編集ビュー・読み書き)

`Pictor/profiles/*.profile.json` を読み書きする宣言的プロファイルエディタ。
正本スキーマは `Pictor/spec/pipeline-profile-config.md` (schema v1)。
保存すると即ディスクへ書き込まれる — このファイル群が Pictor 側で
`register_presets_from_dir()` に読まれる正本。

### プロファイルディレクトリの解決

`profile_store.ts` の `resolveProfileDir()` が以下の順で決定する:

1. 環境変数 `ERGO_PICTOR_PROFILE_DIR` (絶対 / `cwd` 相対)
2. `<cwd>/../../../Pictor/profiles` (ergo と Pictor が sibling の標準配置。
   `cwd` は `tools/ergo` なので `E:/Document/Ars/Pictor/profiles` に解決)
3. `<cwd>/profiles` (最終フォールバック)

### 編集対象

| セクション | UI | 対応スキーマ |
|-----------|----|--------------|
| プロファイル基本 | スカラフィールド | §3.1 (`profile_name` / `rendering_path` / `max_lights` / `msaa_samples` / `gpu_driven_enabled` / `compute_update_enabled`) |
| Render Passes | pass カード列 (追加 / 削除 / ▲▼並べ替え) | §3.2 `RenderPassDef[]` 全フィールド |
| Post Process Stack | effect カード列 (追加 / 削除 / 並べ替え) | §3.3 `PostProcessDef[]` + §3.4 editor 専用 `params` |
| Shadow | フィールドグリッド | §3.5 `ShadowConfig` |
| GI / Lighting | フィールドグリッド + 3 サブグループ | §3.6 `GIConfig` / `ShadowMapConfig` / `SSAOConfig` / `GIProbeConfig` |
| Memory | フィールドグリッド + gpu サブグループ | §3.7 `MemoryConfig` / `GpuMemoryAllocator::Config` |
| GPU Driven | フィールドグリッド | §3.8 `GPUDrivenConfig` |
| Update | フィールドグリッド | §3.9 `UpdateConfig` |
| Profiler | フィールドグリッド | §3.10 `ProfilerConfig` |

最下部に「保存される JSON プレビュー」を常時表示。

### Post-process の editor 専用パラメータ

`PostProcessDef` は C++ 側に `name` / `enabled` しか無い (§3.4)。
それ以外のキー (bloom threshold 等) は `params` バッグとして Editor が
ディスク上で round-trip するが、 C++ シリアライザは黙ってスキップする。
Editor は既存ファイルの追加キーを読み取って `params` 化し、 保存時に
各 effect オブジェクトへ spread し直して JSON に戻す。

### 編集フロー

1. 左ペインの一覧から `*.profile.json` を選択 → フォームにロード
2. 各フィールドを編集 (変更で「未保存」フラグ + 保存ボタン有効化)
3. `💾 保存` → `POST /api/profile` でディスクへ書き込み
4. `＋` で新規プロファイル作成 (`profile_name` から
   `<lowercased-name>.profile.json` のファイル名が保存時に導出される)

別クライアントが保存すると WS で `{op:"profiles-changed"}` がブロードキャスト
され、 未保存の編集が無いクライアントは一覧を静かに再読込する。

---

## 場所

- 実装: `tools/ergo/src/plugins/render_pipeline/`
  - `index.ts` — Hono routes (Scanner API + Profile API) + WS
  - `profile_schema.ts` — `PipelineProfileDef` の TS スキーマ + normalize / serialize
  - `profile_store.ts` — `*.profile.json` のディスク I/O (list / load / save)
  - `ui/index.html` — 2 モード (Scanner + Editor) の UI
  - `ui/styles.css`
  - `ui/app.js` — vis.js Network (Scanner) + プロファイルフォーム (Editor)
- Scanner: `tools/ergo/scanner/render_pipeline_scan.py`
- Snapshot: `tools/ergo/scanner/render_pipeline.json` (生成物、 commit する)
- 登録: `tools/ergo/src/core/registry.ts` の `PLUGIN_FACTORIES`

## URL

| URL | モード | 役割 |
|-----|--------|------|
| `…/render_pipeline/` | — | UI (Scanner がデフォルト) |
| `…/render_pipeline/api/snapshot` | A | scanner スナップショット JSON |
| `…/render_pipeline/api/health` | A+B | snapshot 統計 + プロファイルディレクトリ情報 |
| `…/render_pipeline/api/rescan` | A | scanner を子プロセス起動 |
| `…/render_pipeline/api/profiles` | B | `*.profile.json` の一覧 + ディレクトリ |
| `…/render_pipeline/api/profile/:file` | B | 1 プロファイル読み込み (normalize 済) |
| `…/render_pipeline/api/profile` (POST) | B | プロファイルをディスクへ保存 |
| `ws://…/render_pipeline/ws` | A+B | 保存通知 `{op:"profiles-changed"}` + Phase 2 timing relay |

`POST /api/profile` のボディ: `{ profile, file? }`。 `file` 省略時は
`profile_name` から `<lowercased-name>.profile.json` を導出する。

## 使い方

```sh
# Scanner: Pictor / ergo の現状を JSON 化
cd tools/ergo/scanner
python render_pipeline_scan.py

# ツール起動
cd ../
npm run dev
# → http://localhost:5170/render_pipeline/

# 起動後ヘッダで Scanner / Profile Editor を切り替え
# Profile Editor は Pictor/profiles/*.profile.json を直接編集 + 保存
```

プロファイルディレクトリを明示する場合:

```sh
ERGO_PICTOR_PROFILE_DIR=/path/to/profiles npm run dev
```

## スキーマ同期

`profile_schema.ts` と `ui/app.js` の `ENUMS` / `blankProfile()` は
`Pictor/spec/pipeline-profile-config.md` (schema v1) と
`Pictor/profiles/*.profile.json` のミラー。 Pictor 側スキーマが変わったら
3 箇所 (`profile_schema.ts` / `app.js` / 本ドキュメント) を同時に更新する。

normalize / serialize は C++ シリアライザの前方互換挙動 (§2 / §6) を踏襲:
未知キーは破棄、 欠落キーは既定値、 未知 enum 文字列は既定値フォールバック。
値域検証はしない (C++ 側と同じく `msaa_samples: 3` 等も格納される)。

## Phase 2 (実行時 GPU timing) 設計メモ

Pictor 側に VkQueryPool + timestamp を入れて、 各 pass の end_render_pass
で `vkCmdWriteTimestamp` を呼ぶ。 1 frame ぶんの timestamp 配列を読み戻して
`{op:"timing", frame:N, passes:[{id, us}]}` を WS で publish する。 ergo
プラグインは receive → Scanner 側 vis.js ノードラベルに "scene_hdr (1.23 ms)"
を追加する。

## 既知の制限

### Scanner (モード A)

- scanner の C++ 解析は正規表現ベースなので、 関数境界の判定が雑
- compute pipeline (`vkCreateComputePipelines`) は今は scan していない
- ergo 側の C++ pipeline (gpu_particle 等) も今は scan 対象外
- DAG は `scanner.py` の `PASS_DAG` を編集して手動更新する必要がある

### Profile Editor (モード B)

- **系統B 未配線**: `render_targets` / `input_textures` / `shader_override`
  はファイルに保存されるが Pictor の実 framebuffer / scheduler に届かない
  (Pictor 側正本 §1.2 / §6)。 UI 上もその旨をフィールドヒントに明記
- **post-process パラメータは C++ 不達**: §3.4 のとおり `params` は Editor 内
  round-trip のみ。 `PostProcessDef` 構造体拡張は別タスク
- **値域検証なし**: `msaa_samples` のドロップダウンが `0/2/4/8` を提示する程度。
  無意味値の保存自体は弾かない (C++ シリアライザと同じ方針)
- 並行編集の競合解決はしない。 別クライアントの保存通知で未保存でない側を
  静かに再読込するのみ (last-write-wins)
