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
- `tools/ergo` は **共有プラグインホスト**。core/shell + 汎用プラグイン
  (`particle` / `variable` / `rive` / `profile` / `render_pipeline`) だけを持ち、ゲーム固有のエディタは持たない
- 各プラグインは自身の HTTP ルートと WS ハンドラを提供する
- プラグインの 2 系統:
  - **組み込み (汎用)** — `src/plugins/<id>/` に配置し `src/core/registry.ts`
    に登録。どのホストプロジェクトでも使う `particle` / `variable` / `rive` /
    `profile` / `render_pipeline` のみ
  - **外部 (ゲーム固有)** — ホストリポ側に置き、`ERGO_PLUGIN_DIR` 環境変数で
    指定したディレクトリから起動時に動的ロードする (`src/core/external.ts`)。
    Ergo の core/shell を fork せずに各ゲームが自分のエディタを足せる
    - KuzuSurvivors → `tools/kzs-web/plugins/{spawn,skill}`
    - AdventureCube → `tools/ac-web/plugins/{placer,terrain,acstage}`
- 新しいツールは **新規プラグインとして追加** (新規サーバは作らない)。
  汎用なら組み込み、特定ゲーム向けなら外部プラグインにする

## 起動モード

- **既定: Electron スタンドアロンアプリ** (`npm start` / `editor.bat`)
  - 同プロセスで Node サーバを boot し、`BrowserWindow` がシェル UI を表示
  - ネイティブウィンドウとして扱えて、ブラウザを別に開く必要がない
  - エンジンクライアント (AdventureCube の `ergo_particle` / `ergo_bind`)
    は従来通り `ws://127.0.0.1:5170/...` に接続する (同じポート)
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
├── tsconfig.json
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
    │   └── server.ts     # Hono + http.Server + WS の共通土台
    └── plugins/          # 組み込み (汎用) プラグインのみ
        ├── particle/     # 旧 particle-editor
        │   ├── index.ts
        │   ├── schema.ts
        │   └── ui/
        └── variable/     # 旧 variable-editor
            ├── index.ts
            ├── protocol.ts
            └── ui/
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
- 壊れた pack エントリはログを出してスキップ — 起動は止めない。
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
  - KuzuSurvivors: `tools/kzs-web/plugins/{spawn,skill}`
  - AdventureCube: `tools/ac-web/plugins/{placer,terrain,acstage}`

## 現行プラグイン (組み込み・汎用)

### `particle`
- 旧 `tools/particle-editor/` の移植
- 単一 `ParticleEffectConfig` を保持し、すべての WS 接続に `state` を配信
- ブラウザ UI ↔ engine client (AdventureCube via `ergo_particle`)

### `variable`
- 旧 `tools/variable-editor/` の移植
- `BIND_VAR()` 登録を受ける engine ↔ UI ハブ
- `hello` メッセージでロール (`engine` / `ui`) を識別

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

第1弾ではスモークテストのみ:
- `npm install && npm run start` でサーバが起動すること
- `/api/plugins` が JSON で 2 件返すこと
- `/particle/` / `/variable/` の UI が 200 で返ること
- ws 接続で `state` / `registry` メッセージが受信できること
