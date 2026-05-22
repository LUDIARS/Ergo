# render_pipeline plugin — Render Pipeline Visualizer + Profile Editor

## 概要

Ergo 統合開発者ツールの組み込みプラグイン。 **3 つのモード**を持つ:

| モード | 何 | 種別 |
|--------|----|------|
| **Scanner** (現状ビュー) | Pictor のハードコードされた Vulkan 実装 (系統B) を静的スキャンした render pass **DAG** / pipeline / shader / attachment | **読み取り専用** |
| **Timeline** (実行順ビュー) | Scanner と同じ snapshot の `passes[]` を **実行順のガントチャート**で表示 | **読み取り専用** |
| **Profile Editor** (編集ビュー) | `Pictor/profiles/*.profile.json` (系統A の `PipelineProfileDef`) の編集 | **読み書き + ディスク永続化** |

Scanner と Timeline は同じ scanner snapshot (`render_pipeline.json`) を読む
別表現で、 前者は依存グラフ (DAG)、 後者は実行順タイムライン。 Profile Editor は
「Pictor にどう描かせたいか」という別物であり、 **API 名前空間も対象ファイルも
完全に分離**している。 UI はヘッダのモード切替ボタンで 3 つの `<main>` を
排他表示する (Scanner = 青 / Timeline = ティール / Editor = 紫 で色分け)。

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

## モード T: Timeline (実行順ビュー・読み取り専用)

Scanner と同じ snapshot を**実行順のガントチャート**で描く 3 つ目のモード。
DAG が「依存の形」を見せるのに対し、 Timeline は「pass がどの順で・どこが
並列で流れるか」を時系列バーで見せる。 編集は一切できず、 snapshot 取得 /
Rescan は Scanner モードと共有する (Timeline に切り替えた時に未取得なら自動取得)。

### モデルの組み立て方

UI 側 (`app.js` の `buildTimelineModel()`) が snapshot から純粋計算で組む:

- **実行順 (X 軸)** — `passes[]` の配列順が実行順。 さらに各 pass を
  **topological level** に割る。 `level(p) = max(level(各 consume 先 attachment
  の producer)) + 1`。 `renderDag()` が暗黙に使う level 計算を明示化したもので、
  依存の無い pass は同じ level に並ぶ。 1 level = 横 `TL_LEVEL_W` px。
- **pass バー** — pass 1 本につき 1 バー。 幅は既定で均等。 「draws で重み付け」
  トグルで `draws[]` 本数に比例 (0.5〜2.0× にクランプ)。
- **レーン分け** — 配列順に走査し、 X 区間が既存レーンと重ならない最初の
  レーンに pass を置く。 並列 pass (依存が無く同 level の pass) は別レーンに
  落ちて視覚的に衝突しない。 現 snapshot は直列 4 pass なので 1 レーンだが、
  将来 snapshot が独立 pass を持てば自動でレーンが増える。
- **sub-pass ネスト** — `kind == "graphics_chain"` の pass (postprocess 等) は
  内部の `draws[]` 各要素を pass バー内に小バーとしてネスト表示する
  (Bloom Extract / Blur / ColorGrade / ToneMap など)。 `graphics` kind の
  pass はネストせず draws 本数のみ表示。
- **attachment ライフタイム** — 各 attachment について
  produce する pass の level 〜 最後に consume する pass の level の区間を
  斜縞バーで併記 (下段の専用バンド)。 「attachment ライフタイム」トグルで
  ON/OFF。 ライフタイムバーもレーンパックして横方向に重ねない。

### 表示要素

- **Pass Timeline** — 自前 SVG/div ベースのガントチャート (横スクロール)。
  上端に level 目盛 (`lv 0`, `lv 1` …)、 pass バーをレーンに配置、 下段に
  attachment ライフタイムバンド。 pass バークリックで右ペインに詳細
  (id / kind / consumes / produces / draws / GPU 実測 / description)
- **凡例** — pass バー / graphics_chain / sub-pass / attachment ライフタイム /
  実測 GPU 時間 (Phase 2) の色見本
- **ツールバー** — 「draws で重み付け」「attachment ライフタイム」トグル +
  timing 状態表示 (`timing: 静的` / `timing: 実測 (frame N)`)

ガント描画は外部ライブラリ (vis-timeline 等) を使わず、 絶対配置の `div`
バーで自前実装している (level→px の単純な線形変換だけで足り、 依存追加を
避けるため)。 vis.js は Scanner の DAG だけが使う。

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
| Post Process Stack | effect カード列 (追加 / 削除 / 並べ替え) + kind 別パラメータ編集 | §3.3 / §3.4 `PostProcessDef[]` (typed) |
| Shadow | フィールドグリッド | §3.5 `ShadowConfig` |
| GI / Lighting | フィールドグリッド + 3 サブグループ | §3.6 `GIConfig` / `ShadowMapConfig` / `SSAOConfig` / `GIProbeConfig` |
| Memory | フィールドグリッド + gpu サブグループ | §3.7 `MemoryConfig` / `GpuMemoryAllocator::Config` |
| GPU Driven | フィールドグリッド | §3.8 `GPUDrivenConfig` |
| Update | フィールドグリッド | §3.9 `UpdateConfig` |
| Profiler | フィールドグリッド | §3.10 `ProfilerConfig` |

最下部に「保存される JSON プレビュー」を常時表示。

### Post-process の typed パラメータ (方針2 phase 1)

`PostProcessDef` は `kind` (種別タグ) + エフェクトごとのパラメータ構造体を
持つ (Pictor `pipeline-profile-config.md` §3.4)。Editor は各 effect カードに
`kind` セレクタを置き、選択中の kind に対応する typed なパラメータグリッド
(Bloom / ToneMapping / Vignette / ColorGrading / DepthOfField) を表示する。
`name` 入力からは `kind` を自動推論 (`Bloom` / `Tonemapping` / `LUT` 等)。
`kind` 別パラメータは Pictor の `build_post_process_config()` ブリッジ経由で
実 `PostProcessPipeline` に届く。`SSAO` / `TAA` / `FXAA` / `VolumetricFog`
等は `kind=Unknown` に解決され、パラメータブロックを持たない (host-driven な
`PostProcessPipeline` に実装が無く、ブリッジが無視する = 系統B phase 2)。
保存時は `serializePostProcess()` が `kind` に一致するブロックのみ書き出す。

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
  - `ui/index.html` — 3 モード (Scanner + Timeline + Editor) の UI
  - `ui/styles.css`
  - `ui/app.js` — vis.js Network (Scanner) + 自前ガント (Timeline) +
    プロファイルフォーム (Editor)
- Scanner: `tools/ergo/scanner/render_pipeline_scan.py`
- Snapshot: `tools/ergo/scanner/render_pipeline.json` (生成物、 commit する)
- 登録: `tools/ergo/src/core/registry.ts` の `PLUGIN_FACTORIES`

## URL

| URL | モード | 役割 |
|-----|--------|------|
| `…/render_pipeline/` | — | UI (Scanner がデフォルト、 Timeline / Editor へ切替可) |
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
`{op:"timing", frame:N, passes:[{id, us}]}` を WS で publish する。

### UI 側に用意済みの接続余地 (今回実装)

Timeline モードが実測 timestamp の注入先として既に配線済み。 今回は静的
データだけで動くが、 view 側は以下の seam を持つ:

- `index.ts` の WS `onUpgrade` ハンドラは予約コメント通り `{op:"timing"}`
  を受ける形 (Phase 1 は ping のみ ack)。 Pictor が publish を始めたら
  ここで `clients` へ relay するだけ
- `app.js` の WS クライアント (`connectWs()`) は `op:"timing"` を受けると
  `applyTimingMessage(msg)` を呼ぶ。 `{op:"timing", frame, passes:[{id,us}]}`
  と単発の `{op:"timing", pass, us}` の両形を受理する
- `applyTimingMessage` → `injectTiming(passId, micros)` が `TL_TIMING`
  マップへ格納し、 該当 pass バーに実測時間バー + `1.23 ms` タグを重ねる
  (`applyTimingToBar`)。 最遅 pass を 100% とした相対幅で描画
- timing 注入後はツールバーの状態表示が `timing: 実測 (frame N)` に変わり、
  pass 詳細ペインに `GPU 実測` 行が出る

つまり Phase 2 で必要なのは **Pictor 側の VkQueryPool 実装 + index.ts の
relay 1 行**だけで、 Timeline view 自体は構造変更不要。 静的タイムラインと
実測オーバレイは同じバー上に共存する設計。

## 既知の制限

### Scanner (モード A)

- scanner の C++ 解析は正規表現ベースなので、 関数境界の判定が雑
- compute pipeline (`vkCreateComputePipelines`) は今は scan していない
- ergo 側の C++ pipeline (gpu_particle 等) も今は scan 対象外
- DAG は `scanner.py` の `PASS_DAG` を編集して手動更新する必要がある

### Timeline (モード T)

- バー幅は実測時間ではなく **論理量** (均等 or `draws[]` 本数)。 実時間軸に
  なるのは Phase 2 で GPU timestamp が入ってから。 X 軸の目盛は実行 level で
  あって時間ではない
- topological level は `consumes`/`produces` の attachment 一致だけで判定する。
  attachment を介さない暗黙の順序依存 (同 attachment への LOAD 順など) は
  level に反映されない — `PASS_DAG` の配列順は保たれるが level は同値になりうる
- attachment ライフタイムは level 粒度。 同一 level 内での produce→consume の
  細かい前後関係は表現しない

### Profile Editor (モード B)

- **系統B 未配線**: `render_targets` / `input_textures` / `shader_override`
  はファイルに保存されるが Pictor の実 framebuffer / scheduler に届かない
  (Pictor 側正本 §1.2 / §6)。 UI 上もその旨をフィールドヒントに明記
- **post-process パラメータは C++ に届く** (方針2 phase 1): §3.4 の typed な
  `kind` 別パラメータは `build_post_process_config()` ブリッジ経由で実
  `PostProcessPipeline` に反映される。残るのは任意 pass 挿入 (SSAO/TAA 等の
  固定 4-pass 解体 = 系統B phase 2)
- **値域検証なし**: `msaa_samples` のドロップダウンが `0/2/4/8` を提示する程度。
  無意味値の保存自体は弾かない (C++ シリアライザと同じ方針)
- 並行編集の競合解決はしない。 別クライアントの保存通知で未保存でない側を
  静かに再読込するのみ (last-write-wins)
