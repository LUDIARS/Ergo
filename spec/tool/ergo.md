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
- 各プラグインは `src/plugins/<id>/` に配置され、自身の HTTP ルートと
  WS ハンドラを提供する
- 新しいツールは **新規プラグインとして追加** (新規サーバは作らない)

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
│   └── main.cjs          # Electron main process (boot server + open window)
├── tsconfig.json
├── README.md
├── public/               # シェル UI
│   ├── index.html
│   ├── shell.css
│   └── shell.js
└── src/
    ├── main.ts           # エントリ (port / factories)
    ├── core/
    │   ├── plugin.ts     # Plugin interface
    │   ├── registry.ts   # 組み込みプラグインの列挙
    │   └── server.ts     # Hono + http.Server + WS の共通土台
    └── plugins/
        ├── particle/     # 旧 particle-editor
        │   ├── index.ts
        │   ├── schema.ts
        │   └── ui/
        └── variable/     # 旧 variable-editor
            ├── index.ts
            ├── protocol.ts
            └── ui/
```

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

新規プラグインの追加は `src/core/registry.ts` の配列に factory を 1 行
足すだけ。

## 現行プラグイン

### `particle`
- 旧 `tools/particle-editor/` の移植
- 単一 `ParticleEffectConfig` を保持し、すべての WS 接続に `state` を配信
- ブラウザ UI ↔ engine client (AdventureCube via `ergo_particle`)

### `variable`
- 旧 `tools/variable-editor/` の移植
- `BIND_VAR()` 登録を受ける engine ↔ UI ハブ
- `hello` メッセージでロール (`engine` / `ui`) を識別

## ロードマップ (Phase 2)

### `inspector` プラグイン統合

現状:
- `ergo_inspector` モジュールが C++ 内蔵 POSIX WS サーバで UI を配信
- `tools/inspector_web/index.html` を自前で serve
- Windows は dummy 実装で事実上機能しない

Phase 2 で以下を実施:

1. `src/plugins/inspector/` を新設し、`inspector_web/index.html` を
   `plugins/inspector/ui/` に移動
2. `ergo_inspector` の C++ 側を **アウトバウンド WS クライアント**
   (`ergo_bind` と同じ形) に書き換え、内蔵 POSIX サーバを削除
3. デフォルト接続先を `ws://127.0.0.1:5170/inspector/ws` とする
4. Windows dummy 実装も撤去 (全 OS 同一コードパス)
5. `spec/module/inspector.md` を更新
6. 旧 `tools/inspector_web/` (存在すれば) を削除

既知のリスク:
- `ergo_bind` との機能重複。Phase 2 着手前に「inspector は bind の
  スーパーセットか、別機能を提供するか」を判定する。もし bind の
  サブセットであれば **inspector モジュール自体を廃止** し、variable
  プラグインに一本化する選択肢もある。

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
