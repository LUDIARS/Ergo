# ergo (unified developer tool) 仕様

## 概要

Ergo の開発者向け Web ツール (particle-editor / variable-editor / 今後の
inspector など) を **単一の Node サーバ + プラグインアーキテクチャ** に
統合したツール。`tools/ergo/` に配置。

旧ツールは以下の理由で統合した:

- ツールごとに `package.json` / `tsconfig.json` を保守するのが冗長
- ポートが増え続けるのが開発者側のノイズ (5173, 5174, ...)
- 複数エディタを 1 画面で横断的に使いたいケースがある (ビルド設定を
  切り替えずに WebSocket 経由でリアルタイム編集を並行する)

## 運用方針

- **1 Node プロセス / 1 ポート / 複数プラグイン**
- `tools/ergo` は **共有プラグインホスト**。core/shell + 汎用プラグインだけを持ち、
  ゲーム固有のエディタは持たない
- 各プラグインは自身の HTTP ルートと WS ハンドラを提供する
- プラグインの 2 系統:
  - **組み込み (汎用)** — `src/plugins/<id>/` に配置し `src/core/registry.ts`
    の `PLUGIN_FACTORIES` に登録した、どのホストプロジェクトでも使うもの。
    実在する組み込みプラグインの正本は `registry.ts` で、一覧は下の
    「プラグイン一覧」 (ctest `test_module_catalog` が両者の一致を検査する)
  - **外部 (ゲーム固有)** — ホストリポ側に置き、`ERGO_PLUGIN_DIR` 環境変数で
    指定したディレクトリから起動時に動的ロードする (`src/core/external.ts`)。
    Ergo の core/shell を fork せずに各ゲームが自分のエディタを足せる
    - PrivateGame → `tools/game-web/plugins/{spawn,skill}`
    - AdventureCube → `tools/ac-web/plugins/{placer,terrain,acstage}`
- 新しいツールは **新規プラグインとして追加** (新規サーバは作らない)。
  汎用なら組み込み、特定ゲーム向けなら外部プラグインにする

## 起動モード

- **既定: Electron スタンドアロンアプリ** (`npm start` / `editor.bat`)
  - 同プロセスで Node サーバを boot し、`BrowserWindow` がシェル UI を表示
  - ネイティブウィンドウとして扱えて、ブラウザを別に開く必要がない
  - エンジンクライアント (`ergo_bind` / `ergo_particle_link` / `ergo_scene_link` /
    `ergo_log_live`) は従来通り `ws://127.0.0.1:5170/<id>/ws` に接続する (同じポート)
- **ヘッドレス: `npm run serve` / `npm run dev`**
  - ウィンドウを開かず Node サーバのみ。ブラウザから接続する従来モード
  - CI、Docker、リモート接続、ブラウザ側 devtools を主に使うケースなど

Electron を選んだ理由: バックエンドがフル Node (Hono + `ws`) のため、
Electron は素直にラップするだけで済む。Tauri は Rust バックエンド前提 or
Node sidecar 同梱が必要で、同じ UI 体験を出すのにコストが高い。
将来バックエンドを Rust/WebGPU 化するなら Tauri への乗り換えは検討対象。

## ポートと URL

既定ポート: **5170** (`PORT` 環境変数で上書き可)

| URL                                  | 役割                                            |
|--------------------------------------|------------------------------------------------|
| `http://localhost:5170/`             | シェル UI (プラグイン選択)                        |
| `http://localhost:5170/api/plugins`  | 登録プラグイン一覧                               |
| `http://localhost:5170/api/health`   | 全プラグイン集約ヘルス                           |
| `http://localhost:5170/<id>/`        | プラグイン `<id>` の UI                          |
| `http://localhost:5170/<id>/api/*`   | プラグイン固有の REST                            |
| `ws://localhost:5170/<id>/ws`        | プラグイン固有の WebSocket                        |

## ディレクトリ構造

```
tools/ergo/
├── package.json          # name: "ergo" / main: electron/main.cjs
├── electron/
│   ├── main.cjs          # Electron main process (boot server + open window)
│   └── preload.cjs       # contextBridge: window.ergo.electron (IPC)
├── tsconfig.json         # ビルド (src → dist/)
├── tsconfig.test.json    # 型検査だけ (src + tests、npm run typecheck)
├── README.md
├── public/               # シェル UI
│   ├── index.html
│   ├── shell.css
│   ├── shell.js
│   └── extensions.js     # window.ergo.shell イベントバス (拡張 API)
└── src/
    ├── main.ts           # エントリ (port / 組み込み + 外部 factories)
    ├── core/
    │   ├── plugin.ts     # Plugin interface
    │   ├── shell-api.ts  # window.ergo.shell の TypeScript 型定義 (型のみ)
    │   ├── registry.ts   # 組み込み (汎用) プラグインの列挙
    │   ├── external.ts   # ERGO_PLUGIN_DIR からの外部プラグイン動的ロード
    │   ├── safe_path.ts  # resolveWithin: ルート外へ出るパスを拒む共通ガード (visus / ui_layout)
    │   └── server.ts     # Hono + http.Server + WS の共通土台 (id と形の検査を含む)
    └── plugins/          # 組み込み (汎用) プラグインのみ。1 プラグイン = 1 ディレクトリ
        ├── particle/     # 旧 particle-editor (index.ts / schema.ts / ranges.ts / ui/)
        ├── variable/     # 旧 variable-editor (index.ts / protocol.ts / ui/)
        └── <id>/         # rive / profile / pictor_perf / render_pipeline / visus /
                          # ui_layout / vfx-suggest / vfx-breed / scene_editor / log
tests/                    # npm test (node:test + tsx)。src/ と同じ分け方
├── core/                 # server / external / safe_path
├── helpers/              # fake_ws.ts などテスト用の部品
└── plugins/<id>/         # プラグインごとのテスト
```

ゲーム固有プラグイン (`placer` / `terrain` / `acstage` / `spawn` / `skill`
など) は **このツリーには無い**。各ホストリポの plugin pack に置き、
`ERGO_PLUGIN_DIR` でロードする (後述「外部プラグイン」)。

## Plugin I/F (TypeScript)

`src/core/plugin.ts`:

```ts
interface Plugin {
    id:    string;                       // URL セグメント + WS ルーティングキー
    title: string;                       // シェル表示名
    icon?: string;                       // 絵文字 / SVG
    description?: string;
    routes(ctx: PluginContext): Hono;    // /<id>/* を担当する Hono subapp
    onUpgrade?(req, ws, ctx): void;      // /<id>/ws
    health?(): { ok: boolean; [k:string]: unknown };
}
```

プラグインは **ファクトリ関数** として export:

```ts
export default (): Plugin => {
    // クロージャで状態 (clients, registry, ...) を隠蔽
    return { id: "...", title: "...", routes(), onUpgrade(), health() };
};
```

組み込み (汎用) プラグインの追加は `src/core/registry.ts` の配列に factory を
1 行足すだけ。

## 外部プラグイン (ゲーム固有)

ゲーム固有のエディタは Ergo リポには置かず、**各ホストリポの plugin pack**
に置く。`tools/ergo` の core/shell を fork せず、各ゲームが自分のエディタを
持てるようにするための仕組み。

- 起動時に `ERGO_PLUGIN_DIR` 環境変数 (OS のパス区切りで複数指定可) を読み、
  指定ディレクトリ直下の各サブディレクトリの `index.{js,ts}` を動的 import
  して `default` export (`PluginFactory`) を組み込み factory に併合する
  (`src/core/external.ts`)。
- 壊れた pack エントリはログを出してスキップ — 起動は止めない (下の「プラグイン id と
  読み込みの規則」)。
- TypeScript 製プラグインは `npm run serve` / `dev` (tsx) でそのままロード。
  Electron (`npm start`) 経路でも `electron/main.cjs` が tsx の ESM ローダを
  register するので `.ts` のままロードできる。
- **plugin pack の作り方** (ホストリポ側):
  - `<host>/tools/<name>-web/plugins/` を作り、各プラグインを
    `<id>/index.ts` (+ `schema.ts` / `store.ts` / `ui/`) として置く
  - プラグイン契約は Ergo の `core/plugin.ts` を `plugins/_contract.ts` に
    コピーして `import type` で使う (ツール側は返り値の構造を duck-type
    するのでランタイム結合は無い)
  - `staticRoot` は cwd 相対ではなく
    `fileURLToPath(new URL("./ui", import.meta.url))` で自分の絶対パスにする
    (外部ロード時の cwd はホスト側でないため)
  - `plugins/package.json` に pack が import する依存 (`hono` / `ws`) だけ
    宣言し、ホストの `editor.bat` が pack で `npm install` してから
    `ERGO_PLUGIN_DIR` を渡してツールを起動する
- 既存の plugin pack:
  - PrivateGame: `tools/game-web/plugins/{spawn,skill}`
  - AdventureCube: `tools/ac-web/plugins/{placer,terrain,acstage}`

### プラグイン id と読み込みの規則

組み込み・外部の両方に同じ規則を当てる (`src/core/server.ts` の
`instantiatePlugins()` / `createApp()`、`src/core/external.ts`)。

- **id**: `/^[a-z0-9][a-z0-9_-]*$/` に合うこと (URL の 1 セグメントとルートの接頭辞に
  なる)。`api` と `shell` はサーバ自身のルートなので使えない
- **形**: `title` は文字列。`description` / `icon` / `staticRoot` は、あれば文字列。
  `routes` / `onUpgrade` / `health` は、あれば関数
- **壊れたプラグインの扱い**: 組み込みが壊れていたら起動を止める (このリポのバグ)。
  外部は、ファクトリや `routes()` が例外を投げた・形が違う・id が重複した、の
  どれでもログを出してスキップする。pack エントリの import に失敗したもの、
  `default` (または `factory`) の関数 export が無いものもログを出してスキップする
- **id の重複**: 先に名乗ったものが勝つ。組み込みが外部より先で、外部は
  `ERGO_PLUGIN_DIR` に並べた順、pack の中ではエントリ名の順
- **`/api/health`**: ファクトリ・形・重複・`routes()` で外したものを
  `rejected: [{id, source, ok:false, err}]` に並べ、そのときは全体の `ok` も false。
  `health()` が例外を投げた・真偽値の `ok` を返さないプラグインは、`plugins` の中で
  `ok:false` と `err` 付きで表示する (`id` / `title` は `health()` で上書きできない)
- **`index.js` と `index.ts`**: 両方あれば、どの起動経路 (tsx の `serve` / `dev`、
  Electron の `dist/`) でも `index.js` を読む

組み込みプラグインを足すときは、`tests/core/server.test.ts` が `PLUGIN_FACTORIES` の
全部について id の形・`/<id>/` の `index.html` が 200・`GET /<id>/api/health` が
200 で真偽値の `ok` を返すことを検査する。

## プラグイン一覧

`src/core/registry.ts` の `PLUGIN_FACTORIES` に登録された組み込みプラグインの全部
(12 個)。「供給元」はプラグインにデータを送る側で、次の 4 種類:

- **リポ内クライアント**: Ergo のターゲットがエンジン側から WS で接続する
- **ホスト提供**: ホスト (ゲーム・Pictor を回すプログラム) が自分で送る / 自分で HTTP を出す。Ergo にクライアントは無い
- **ファイル**: ブラウザでのファイル読み込みか、環境変数で指したディレクトリ
- **ブラウザのみ**: サーバは状態を持たない (必要なら LLM を呼ぶだけ)

どのプラグインも `GET /<id>/api/health` を持ち、UI は `/<id>/` で開く。

<!-- plugin-matrix:begin -->
| id | C++ モジュール | 供給元 | ワイヤ契約と主なエンドポイント | 仕様 |
|---|---|---|---|---|
| `particle` | `ergo_particle` | リポ内クライアント: `ergo_particle_link` (受信のみ、`include/ergo/particle/editor_link.h`) | WS `/particle/ws`: UI が `set` / `replace` を送り、全接続へ `state` を配信。REST `GET` / `POST /particle/api/effect` | 本書「`particle`」、[particle.md](../module/particle.md) |
| `variable` | `ergo_bind` (`ergo_actor` の変数も) | リポ内クライアント: `ergo::bind::Engine::connect` (既定 `127.0.0.1:5170`、`/variable/ws`) | WS `/variable/ws`: `hello` でロール (`engine` / `ui`) を名乗り、登録と値を中継。REST `GET /variable/api/vars`・`/api/actors` | 本書「`variable`」、[bind.md](../module/bind.md) |
| `rive` | — | ブラウザのみ (`.riv` はブラウザで読む) | WS `/rive/ws`: `publish` (メタ情報) を全接続へ `meta` で配信。REST `GET /rive/api/meta` | [rive.md](rive.md) |
| `profile` | `ergo_profile` | ファイル (`ergo_profile` の Chrome Trace JSON をブラウザに渡す)、またはホスト提供の WS `{op:"publish", trace}` | WS `/profile/ws`: `publish` を全接続へ `trace` で配信 | 本書「`profile`」、[profile.md](../module/profile.md) |
| `pictor_perf` | — (Pictor の `PerfQueryAPI`) | ホスト提供: WS `{op:"publish", snapshot}` か `POST /pictor_perf/api/publish` | WS `/pictor_perf/ws` へ `snapshot` を配信。REST `GET /pictor_perf/api/snapshot` | [pictor_perf.md](pictor_perf.md) |
| `render_pipeline` | — (Pictor のプロファイル) | ファイル: `ERGO_PICTOR_PROFILE_DIR` の `*.profile.json`。GPU 時間はホスト提供の WS `{op:"timing"}` | REST `GET /render_pipeline/api/profiles`・`/api/profile/:file`、`POST /api/profile`。WS: 保存で `profiles-changed`、`timing` を中継 | [render_pipeline.md](render_pipeline.md) / [render_pipeline_system_b.md](render_pipeline_system_b.md) |
| `visus` | — (Pictor の描画定義) | ファイル: `VISUS_PROJECT_ROOTS` 配下の `*.visus.json` | REST `GET /visus/api/roots`・`/api/scan`・`/api/load`、`POST /api/save` (ルートの外は拒否)。WS: 保存を通知 | 本書「`visus`」 |
| `ui_layout` | `ergo_ui_layout` | ホスト提供: ホストが `POST /ui_layout/patch` と `GET /ui_layout/state` を自分で出す (既定 `http://127.0.0.1:8080`、`POST /ui_layout/api/bridge/config` で変更)。`ergo_custos` はこの 2 つを提供しない | REST `/ui_layout/api/bridge/{config,patch,pull}`・`/api/file/{open,save,svg}`。WS `/ui_layout/ws` へ `patch` / `document` を配信 | 本書「`ui_layout`」、[ui_layout.md](../module/ui_layout.md) |
| `vfx-suggest` | `ergo_particle` (設定の形) | ブラウザのみ + 任意で LLM (`ERGO_VFX_BACKEND`) | REST `GET /vfx-suggest/api/games`・`/api/catalog`、`POST /api/generate`・`/api/ks-emitter` (`ks-emitter` は legacy の L2)。WS なし | 本書「`vfx-suggest`」 |
| `vfx-breed` | `ergo_particle` (設定の形) | ブラウザのみ + 任意で LLM (方向指示) | REST `POST /vfx-breed/api/generation`、`GET /api/seeds`・`/api/genes`。WS なし | 本書「`vfx-breed`」 |
| `scene_editor` | `ergo_scene` | リポ内クライアント: `ergo_scene_link` (`include/ergo/scene/editor_link.h`) | WS `/scene_editor/ws`: エンジンが `{op:"scene", id, json}` を送り、UI の `{op:"patch", scene_id, actor_id, field, value}` をそのシーンの持ち主へ転送。REST `GET /scene_editor/api/scenes` | [scene_editor.md](scene_editor.md) |
| `log` | `ergo_log` | リポ内クライアント: `ergo_log_live` (`include/ergo/log/live_sink.h`) | WS `/log/ws`: エンジンが `hello` と `lines` を送り、UI は接続時に `backlog`、以後 `lines`。REST `GET /log/api/apps`・`/api/lines` | [log.md](log.md) |
<!-- plugin-matrix:end -->

ポートを変えた場合 (`PORT`) は、エンジン側クライアントの `connect` にも同じポートを渡す。

## 現行プラグイン (組み込み・汎用)

### `particle`
- 旧 `tools/particle-editor/` の移植
- 単一 `ParticleEffectConfig` を保持し、すべての WS 接続に `state` を配信
- ブラウザ UI ↔ engine client (`ergo_particle_link`。`ergo_particle` 本体はソケットを持たない)
- プレビュー描画の emit / step / draw は `ui/particle-simulation.js` (粒子の計算) と
  `ui/particle-draw.js` (canvas 描画) に分け、`ui/preview.js` (`/particle/preview.js`) が
  ES module として公開する。エディタと `vfx-breed` / `vfx-suggest` の埋め込み
  プレビューが共有し、粒子の配列・発生の時計・フレームループは呼ぶ側がそれぞれ持つ
  (テスト `tests/plugins/particle/preview.test.ts`)

### `variable`
- 旧 `tools/variable-editor/` の移植
- `BIND_VAR()` 登録を受ける engine ↔ UI ハブ
- `hello` メッセージでロール (`engine` / `ui`) を識別

### `rive`
- `.riv` をブラウザの canvas で再生し、アートボード / state machine / アニメーションを見る。
  ファイルはブラウザで読み、サーバは開いているファイルのメタ情報を中継するだけ。
  仕様は [rive.md](rive.md)

### `profile`
- `ergo_profile` が書いた Chrome Trace Event JSON をタイムライン表示する
  (フレームグラフ + カウンタトラック)。トレースはブラウザで読み、WS の `publish`
  は直近のトレースを他の接続へ配るだけ。仕様は [profile.md](../module/profile.md)
  の「タイムライン UI」

### `pictor_perf`
- Pictor の `PerfQueryAPI::export_json()` の性能スナップショット (SoA のアライメント・
  帯域・バッチ適格性・GPU 時間) を表示する。ホストが WS か HTTP で publish する。
  仕様は [pictor_perf.md](pictor_perf.md)

### `render_pipeline`
- Pictor の `*.profile.json` (`ERGO_PICTOR_PROFILE_DIR`) を単一のノードグラフで表示・編集し、
  保存する。仕様は [render_pipeline.md](render_pipeline.md) と
  [render_pipeline_system_b.md](render_pipeline_system_b.md)、ディレクトリの指定は
  [plugin-data-roots.md](../../setup/plugin-data-roots.md)

### `visus`
- 各プロジェクトの `assets/visus/*.visus.json` (Pictor の描画定義) を一覧・編集する。
  ルートは `VISUS_PROJECT_ROOTS` (`;` 区切り、未設定なら cwd)。全 API は
  `core/safe_path.ts` の `resolveWithin` でルートの外へ出るパスを拒む。`geometry.kind ==
  "custom"` はフォームで、それ以外は生 JSON で編集する

### `ui_layout`
- `ergo_ui_layout` の `*.uilayout.json` を Figma 風に編集する (キャンバス・階層・
  プロパティ・ファイル入出力。ファイルはツールの cwd の外へ出られない)
- 実行中のゲームとの往復は **ホストが出す HTTP** を経由する: 編集は
  `POST /ui_layout/api/bridge/patch` → ホストの `/ui_layout/patch`、取り込みは
  `POST /ui_layout/api/bridge/pull` → ホストの `/ui_layout/state`。既定の転送先は
  `http://127.0.0.1:8080` で、`POST /ui_layout/api/bridge/config` で変える。
  `ergo_custos` のブリッジは `/health` / `/screenshot` / `/key` だけで、この 2 つは
  提供しない。仕様は [ui_layout.md](../module/ui_layout.md)

### `vfx-suggest`
- ゲームとシーンの文脈から LLM で `ParticleEffectConfig` のプリセットを作る
  (`POST /vfx-suggest/api/generate`)。結果は `particle/ranges.ts` の値域で丸め (sanitize)、
  プレビューは `particle` プラグインの WS へ送る。LLM のバックエンドは
  `ERGO_VFX_BACKEND` (`cli` / `sdk`) と `ERGO_VFX_MODEL`
  ([config-reference.md](../../setup/config-reference.md))
- `ks_export.ts` (`POST /vfx-suggest/api/ks-emitter`) は特定タイトル向けの書き出しで、
  ホストの plugin pack へ移す予定の legacy (`spec/architecture/domains-and-ux.md` §5 の L2)

### `vfx-breed`
- エフェクトを「選んで育てる」対話的進化。 変異体を並べ、 人間が選んだ個体を親に
  次世代を作って `ParticleEffectConfig` を詰める。 適応度は人間の目そのもので、
  自動の品質指標・画像類似度は持たない。 `vfx-suggest` (文章 → LLM → 設定) と並ぶ
  もう 1 つの作り方で、 両者は排他ではない (vfx-suggest の結果を起点にできる)
- 配置: `tools/ergo/src/plugins/vfx-breed/`。 schema は `particle/schema.ts` の
  `ParticleEffectConfig` をそのまま使う。 数値の値域は `particle/ranges.ts` の 1 表を
  vfx-suggest の sanitize と共有する (重複実装しない)
- 遺伝子 (`genes.ts` / `genome.ts`): 数値項目 (emission / initial / overLife /
  forces) を値域で 0..1 に正規化したもの。 `emission.rate` / `maxAlive` は対数尺度、
  発射方向と色相は環状 (0 と 1 が同じ)。 色 3 つ (`initial.color` /
  `overLife.colorStart` / `colorEnd`) は HSV + alpha の 4 遺伝子ずつ。
  `render.blend` / `render.shape` は離散遺伝子。 `positionShape` / `svgSource` /
  `svgScale` は遺伝子にせず親から引き継ぐ。 戻すときは値域に丸め、
  `speedMin ≤ speedMax`・`lifetimeMin ≤ lifetimeMax` を保ち、 `maxAlive` は整数にする
- 世代 (`breed.ts`): 既定 8 体 (4〜16)。 親が 1 体なら変異だけ、 2 体以上なら
  子ごとに相異なる 2 親を選んで一様交叉 → 変異。 変異 (`mutate.ts`) は正規化空間で
  `strength × (bias + N(0,1))`、 離散遺伝子は `min(0.25, 0.5 × strength)` の確率で
  反転。 乱数は seed 付きの決定的 RNG (mulberry32) で、 同じ入力と seed なら同じ子。
  seed を省くとサーバが決めて応答に載せる
- 方向指示 (`direction.ts` / `advisor*.ts`): 自由文 (例「もっと鋭く短く」) が付いた
  ときだけ、 LLM に「どの遺伝子をどちらへ動かすか」 の重み (-1..1) だけを返させ、
  変異の平均をずらす。 LLM は値そのものを作らない。 バックエンドは vfx-suggest と同じ
  `ERGO_VFX_BACKEND` (`cli` / `sdk`) / `ERGO_VFX_MODEL`。 LLM が無い・失敗したときは
  `direction.status` が `unavailable` / `failed` になるだけで、 進化はそのまま動く
- API (サーバは状態を持たない。 履歴はブラウザ側):
  - `POST /vfx-breed/api/generation` — `{ parents, selected?, direction?, strength?,
    seed?, count?, generation? }` → `{ ok, children: [{ config, parents }], seed,
    generation, direction: { status, weights?, reason? } }`。 `selected` は parents の
    添字 (省略で全部)、 親が無ければ既定エフェクトが親
  - `GET /vfx-breed/api/seeds` — 起点 (既定エフェクト + vfx-suggest のカタログ)
  - `GET /vfx-breed/api/genes` — 遺伝子表 (id / 値域 / 尺度)
  - `GET /vfx-breed/api/health` — 方向指示 (LLM) の状態
- UI (`ui/`): 起点 (既定 / カタログ / particle エディタの現在値 / JSON 貼り付け) →
  世代のグリッド (各セルでブラウザ内プレビュー)、 複数選択、 方向の自由文、 変異の
  強さ、 体数、 seed、 次世代ボタン、 世代履歴 (戻れる。 古い世代から育てると以降の
  履歴は置き換わる)。 各個体は particle プラグインの WS (`replace`) へ送ってエンジン
  側で即プレビュー、 JSON 書き出し、 vfx-suggest の `ks-emitter` での書き出しができる
- プレビュー描画は particle エディタの emit / step / draw を ES module にした
  `particle/ui/preview.js` (`/particle/preview.js`) を使う。 出現位置は disc のみで、
  svgStroke / svgFill は disc で代用する (ergo_particle と同じ)
- テスト: `tools/ergo/tests/plugins/vfx-breed/` (`npm test`、 node:test + tsx)

### `scene_editor`
- `ergo_scene` のシーンをツリー表示し、アクターのフィールドを編集する。エンジン
  (`ergo_scene_link`) が publish したシーンだけを見せ、UI のパッチはそのシーンを
  publish したエンジンへ転送する。エンジンが `Scene::apply_patch` で適用して
  publish し直すので、UI は常にエンジンの状態を表示する。仕様は
  [scene_editor.md](scene_editor.md)

### `log`
- 動作中の `ergo_log` の行を表示する。エンジン (`ergo_log_live` の `LiveSink`) ごとに
  直近 10k 行のリングを持ち、切断後も残す。レベル・文字列・フレーム範囲で絞り込む。
  仕様は [log.md](log.md)

> `placer` / `terrain` / `acstage` は AdventureCube 固有のため Ergo から
> 分離し、`AdventureCube/tools/ac-web/plugins/` の plugin pack へ移設した。
> 仕様は AC リポ側を参照。

## シェル拡張 API (`window.ergo.shell`)

シェル UI (Electron ウィンドウ全体 + サイドバー) に対する拡張ポイント。
カスタム動作 (例: 別パネルの追加、特定プラグイン選択時の自動化) を、
`shell.js` を fork せずに足せるようにするためのイベントバス。

実装は `tools/ergo/public/extensions.js` (DOM script として `shell.js` より
**先に** ロード)、TypeScript 型は `tools/ergo/src/core/shell-api.ts`。

### イベント

| event 名 | 発火タイミング | payload |
|----------|---------------|---------|
| `shell:ready`        | 初期 plugins ロード直後 | `{ plugins: ShellPluginInfo[] }` |
| `plugin:registered`  | 各プラグインごと、初期ロード時 | `{ id, plugin }` |
| `plugin:activated`   | サイドバーで選択された時 (初期表示も含む) | `{ id, plugin }` |
| `plugin:deactivated` | 別プラグインへ切替直前 | `{ id, plugin }` |
| `plugin:health`      | /api/health ポーリング (2 秒) ごと、各プラグイン | `{ id, health }` |
| `plugin:event`       | iframe 内プラグインからの `postMessage` を再発信 | `{ id, name, payload }` |

### API

```js
// シェルページ内に追加した <script> から:
window.ergo.shell.on("plugin:activated", ({ id, plugin }) => {
    console.log("now showing:", plugin.title);
});

const off = window.ergo.shell.on("plugin:event", ({ name, payload }) => {
    if (name === "selection") highlightInToolbar(payload);
});
off();   // 解除

window.ergo.shell.emit("plugin:event", { id: "x", name: "...", payload: {...} });
```

### iframe → shell ブリッジ

各プラグインの iframe 内 UI から、選択イベントなどを shell に伝播する場合:

```js
// プラグイン UI 内 (例: variable プラグイン UI で actor が選ばれた時)
window.parent.postMessage(
    { type: "ergo:plugin:event", name: "actor:selected", payload: { handle: 42 } },
    "*"
);
```

shell は `plugin:event` として再発信し、`id` は **その時点でアクティブな
プラグインの id** が自動補完される。

### Electron IPC

Electron アプリとして起動した場合のみ、preload 経由で
`window.ergo.electron` が露出する。許可済みチャネル:

| 方向 | チャネル | 用途 |
|------|---------|------|
| 送信 | `shell:ready`            | 初回プラグイン数を main へ通知 |
| 送信 | `shell:plugin-activated` | 選択中プラグインを main へ通知 |

`extensions.js` のデフォルト動作は **`plugin:activated` を IPC に転送**
するだけ。`electron/main.cjs` 側は受信したらウィンドウタイトルを
`ergo — <plugin title>` に更新する。新しい IPC チャネルが必要なら
`electron/preload.cjs` の `ALLOWED_OUTBOUND` / `ALLOWED_INBOUND` に追加し、
`main.cjs` で `ipcMain.on(...)` を増やす。

### 拡張の追加方法

1. **シェル内のみのカスタムロジック** — `public/` に `myext.js` を置き、
   `index.html` の `<script>` リストに追加 (extensions.js の後ろ)。
   `window.ergo.shell.on(...)` で必要なイベントを購読する。
2. **メインプロセス連携が必要な場合** — `electron/preload.cjs` の
   ALLOWED 集合に新チャネルを追加し、`main.cjs` でハンドラを書く。

## 廃止プラン (履歴) — `inspector` プラグイン統合は実施せず

当初は `inspector` を tools/ergo プラグインとして取り込む案 (Phase 2) を
検討したが、機能調査の結果 **`ergo_inspector` は `ergo_bind` の完全な
サブセット** と判定し、2026-04-21 に **inspector モジュール自体を廃止**
した。

| 機能 | bind | (旧) inspector |
|------|------|----------------|
| 型消し変数登録 | ✅ | ✅ |
| メタデータ (min/max/step/unit/read_only) | ✅ | ✅ |
| 値変化検出 → 自動 broadcast | ✅ (apply_pending_writes 経由) | ✅ |
| Actor tree 階層 | ✅ | ❌ |
| Win32 含む全 OS で同等動作 | ✅ | ❌ (Windows は dummy) |
| WS サーバ位置 | アウトバウンド (tools/ergo) | エンジン内蔵 POSIX |

新規ライブチューニング機能は `ergo_bind` + `tools/ergo/src/plugins/variable/`
に追加する。`tools/inspector_web/` は本廃止以前から既に未配備。

## 互換性

- 旧 `tools/particle-editor/` / `tools/variable-editor/` は **削除済み**
  (2026-04-20)。旧ポート (5173 / 5174) での接続試行は到達できないため、
  ホストアプリの接続先も同時に更新する必要がある。
- `ergo_bind::Engine::connect(...)` のデフォルト引数を
  `(host, 5170, "/variable/ws")` に変更。旧デフォルト (5174, "/ws") を
  使っていた呼び出し側は更新が必要。

## テスト

`npm test` (node:test + tsx、`tests/**/*.test.ts`) と `npm run typecheck`
(`tsconfig.test.json` で `src/` と `tests/` を型検査、出力なし)。どちらも
`lib/lapilli` の submodule が要る。テストは hermetic で、一時ディレクトリと
ポート 0 を使い、外部のサービスやネットワークに出ない。

| 場所 | 対象 |
|---|---|
| `tests/core/server.test.ts` | 組み込み全部の id・UI・`/api/health`、`/api/plugins`、id と形の検査、壊れた外部プラグインのスキップと `rejected` |
| `tests/core/external.test.ts` | `ERGO_PLUGIN_DIR` の pack 読み込み (複数ディレクトリ、`index.js` 優先、壊れたエントリと id の衝突) |
| `tests/core/safe_path.test.ts` | `resolveWithin` (ルートの外へ出るパスの拒否、Windows のドライブ違いと UNC を含む) |
| `tests/helpers/` | `fake_ws.ts` などテスト用の部品 (テストそのものではない) |
| `tests/plugins/particle/` | 共有プレビュー (`preview.js`) の上限・発生位置・減衰と重力・寿命・描画 |
| `tests/plugins/variable/` | engine ↔ UI の中継 |
| `tests/plugins/pictor_perf/` | publish (WS / HTTP) とスナップショットの検証 |
| `tests/plugins/visus/` | save の検証 (パス・拡張子・JSON) と scan / load |
| `tests/plugins/ui_layout/` | ファイルの保存・open・svg とルートの外へ出るパスの拒否 |
| `tests/plugins/vfx-suggest/` | sanitize (値域への丸め) |
| `tests/plugins/vfx-breed/` | 遺伝子・世代・交叉と変異 (`helpers.ts` は共通部品) |
| `tests/plugins/scene_editor/` | シーンの解析・中継・回転の変換 |
| `tests/plugins/log/` | リング・プロトコル・中継 |

CI ではジョブ `tools-ergo` (`.github/workflows/ci.yml`) が Node 22 で
`npm ci` → `npm run typecheck` → `npm run build` → `npm test` を実行する
(submodule 込み、`ELECTRON_SKIP_BINARY_DOWNLOAD=1`)。手元での再現は
[ci.md](../../setup/ci.md)。
