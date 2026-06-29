# Ergo Redesign — 宣言的 UI 化 + Custos 統合 設計書

> **状態**: 設計フェーズ (2026-05-23 起草)。 実装は別セッション。
> **対象**: Ergo (本リポ) + Custos (LUDIARS/Custos)。 統合後 Custos は obsolete。
> **本書の位置付け**: 既存 `spec/tool/ergo.md` を将来上書きする予定の **新仕様書**。
> 移行完了までは並存し、 plugin 単位で旧仕様 → 新仕様へ段階移行する。

## 1. 背景と問題意識

### 1.1 動機

- **Pictor アプリの実機内で Ergo 系ツール (particle / variable / profile / render_pipeline 等) を
  ダイレクトに見たい場面が増えた**。 現状はブラウザを別ウィンドウで開いて確認している。
- **Custos と Ergo の機能重複** が大きい (両方とも Hono + WS + Electron + plugin 構造を持つ)。
  Custos は別プロセスのため、 ergo_custos in-app HTTP ブリッジを噛ませても余分な round-trip がある。
- **既存 ergo の Web フロントは Vanilla JS + 静的 HTML/CSS** (`tools/ergo/src/plugins/<id>/ui/` 配下)。
  ブラウザでしか描画できず、 in-proc 描画は不可能。

### 1.2 現状アーキ (簡略)

```
Pictor / AdventureCube / KuzuSurvivors (C++ アプリ)
   ├─ ergo_particle ──WS──→ ergo :5170 /particle/ws
   ├─ ergo_bind     ──WS──→ ergo :5170 /variable/ws
   └─ ergo_custos   ←─HTTP── Custos :4649 (apps.json, capture, input)
                                  │
                                  ├─ Web UI (Custos public/, Vanilla JS)
                                  ├─ Electron wrapper
                                  ├─ nut-js + ffmpeg (外部依存)
                                  └─ WebRTC broker (werift)

tools/ergo (Hono :5170)
   ├─ plugins: particle / variable / rive / profile / render_pipeline
   ├─ Vanilla JS UI (per-plugin static HTML)
   └─ Electron wrapper
```

問題点:

| 観点 | 現状 | 課題 |
|------|------|------|
| Pictor 内表示 | 不可 (HTML 直依存) | 別ウィンドウで都度確認、 開発効率低下 |
| プロセス数 | Custos + Ergo + Pictor + (ergo_custos in-app) | オーバーヘッド大、 forwarder 3-way の if 連鎖 |
| 認証 | Custos のみ Cernere 連携、 Ergo は素通し | LUDIARS の per-user / memory-only secret 方針 [[feedback_secret_per_user_memory_only]] とのギャップ |
| UI 重複 | Custos UI + Ergo UI で 2 系統 | Foundation UI / Corpus 宣言レンダリング思想と乖離 |

## 2. ゴールと非ゴール

### 2.1 ゴール

1. **同一 UI 定義から Web ブラウザと Pictor 内の両方で UI を描画できる** (宣言的 UI / IR)
2. **Custos の全機能を Ergo の plugin として吸収し、 Custos リポを obsolete 化する**
3. **既存 5 plugin の UI を壊さない** (1 plugin ずつ段階移植可能、 並存期間あり)
4. **ergo_custos を in-app 双方向ブリッジに進化させる** (HTTP only → +WebSocket、 IR consumer 機能)
5. **Cernere 認証を Ergo 全体に統一** (per-user project-token、 memory-only)
6. **Pictor リポを無変更で実現する** (Pictor 側の並行作業 / リリースサイクルと完全分離)

### 2.2 非ゴール

- 外部 plugin pack (kzs-web / ac-web) の即時 IR 化 (移行ガイドのみ提供、 実移植は各リポ側)
- **Pictor リポへの新規モジュール追加 / 既存ファイル変更** (全 GUI 機能を Ergo 側に集約することで回避)
- Pictor の既存描画系 (Vulkan / Visus / Material) の再設計
- Web 標準準拠の HTML/CSS subset レンダラ実装 (= IR は HTML ではなく独自 schema)
- iOS / Web 配信版 Pictor 対応 (デスクトップ前提)

## 3. 新アーキ概観

```
Pictor / AdventureCube / KuzuSurvivors (C++ アプリ)
   │   ※ Pictor リポ自体は無変更。 すべての GUI 機能は Ergo 側に集約
   │
   ├─ ergo_ui_native (新設、 Ergo の C++ モジュール、 opt-in link)
   │     ・Ergo :5170 と HTTP/WS で接続し UI IR を pull
   │     ・GUI primitives (text / rect / image / layout / input / state) を内蔵
   │     ・描画は Pictor の既存描画 API (Vulkan サブパス / draw 呼び出し) を呼ぶだけ
   │       (Pictor 側に新規モジュールや依存は追加しない)
   │     ・Pictor がプリミティブを欠く場合は ergo_ui_native 側で軽量 helper を内製
   │     ・bindings / actions の WS 中継 (同期更新 / イベント送信)
   │
   ├─ ergo_custos (拡張、 既存)
   │     ・現状: HTTP /health /screenshot /key
   │     ・拡張: +WS streaming、 +mouse、 +Cernere token verify
   │
   └─ ergo_particle / ergo_bind / ... (現状維持)

      ↑↓ HTTP/WS                                  ↑↓ HTTP/WS
      │                                            │
┌─────────────── tools/ergo (Hono :5170) ──────────────────┐
│                                                           │
│  core: server + WS upgrade + plugin registry              │
│   ├─ Cernere middleware (5 分キャッシュ、 dev は ERGO_OPEN=1) │
│   └─ /api/plugins, /api/health                            │
│                                                           │
│  既存 plugins (Phase 3 で IR 化):                          │
│   ├─ particle / variable / rive / profile / render_pipeline│
│                                                           │
│  新規 plugins (Phase 2 で Custos から移植):                │
│   ├─ apps      — apps.json 駆動の build/run/test/kill      │
│   ├─ capture   — WebRTC broker + ergo_custos + ffmpeg     │
│   └─ input     — ergo_custos > nut-js > adb の strategy   │
│                                                           │
│  IR endpoint (各 plugin が提供):                           │
│   GET  /<plugin>/ir       → UI declaration JSON           │
│   WS   /<plugin>/ws       → bindings/actions/state sync   │
│                                                           │
└───────────────────────────────────────────────────────────┘
      ↑                                            ↑
      │ HTTP/WS                                    │ HTTP/WS
      │                                            │
   外部ブラウザ                              Pictor 内 ergo_ui_native
   (React renderer + Foundation UI)         (in-proc renderer)
   同じ IR を描画                            同じ IR を描画
```

### 3.1 Pictor 非干渉の原則 (重要)

- **Pictor リポ (ファイル / モジュール / 依存) は一切変更しない**
- GUI primitives (text / rect / layout / hit-test / state ストア) は **全て Ergo 側 `ergo_ui_native` に内蔵**
- `ergo_ui_native` は Pictor の **既存描画 API のみを opt-in で呼ぶ**
  (Pictor は呼ばれる側、 ergo_ui_native の存在を知らない)
- Pictor が必要なプリミティブを欠く場合は **Ergo 側で軽量 2D ヘルパー (Vulkan サブパスや独自シェーダ)
  を内製** して埋める。 Pictor 本体に primitive を追加しない
- アプリ側 (Pictor demo / AdventureCube / KuzuSurvivors) が
  `target_link_libraries(myapp PRIVATE ergo_ui_native)` した場合のみ in-proc UI が有効化される。
  link しないアプリでは何も起こらない
- [[feedback_pictor_no_upper_dep]] (Pictor は上位ライブラリに依存しない) を完全に維持
- 別セッションが Pictor リポを並行で触っていても **0 衝突**

## 4. UI IR Schema

### 4.1 IR の基本構造

```jsonc
{
  "version": 1,
  "plugin": "particle",
  "root": {
    "kind": "panel",
    "props": { "title": "Particle Editor" },
    "children": [
      {
        "kind": "slider",
        "id": "lifetime-slider",
        "props": { "label": "Lifetime", "min": 0, "max": 10, "step": 0.1 },
        "bindings": { "value": "particle.config.lifetime" }
      },
      {
        "kind": "button",
        "props": { "label": "Spawn" },
        "actions": { "click": { "op": "particle.spawn", "args": {} } }
      }
    ]
  }
}
```

### 4.2 サポート kind (Phase 1 ミニマム)

| カテゴリ | kind |
|---------|------|
| レイアウト | `panel`, `row`, `col`, `tab`, `split`, `scroll` |
| 表示 | `text`, `label`, `icon`, `image`, `badge`, `divider` |
| 入力 | `input`, `textarea`, `slider`, `toggle`, `select`, `button`, `color` |
| 構造 | `tree`, `table`, `list` |
| 描画 | `canvas`, `graph`, `chart`, `timeline` |

`canvas` / `graph` / `chart` / `timeline` は **renderer 側に専用実装が必要** な特殊 kind。
Phase 1 では `canvas` のみ最小実装 (raw pixel array push)、 他は Phase 3 の plugin 移植時に追加。

### 4.3 bindings / actions

- **bindings**: WS で双方向同期される値参照
  ```jsonc
  "bindings": { "value": "particle.config.lifetime", "disabled": "particle.locked" }
  ```
  - サーバ側がいつでも `value` を更新可能 (push)
  - クライアント側の編集も WS で `op: set` メッセージとしてサーバへ送信

- **actions**: ユーザ操作で発火するイベント
  ```jsonc
  "actions": { "click": { "op": "particle.spawn", "args": { "count": 10 } } }
  ```
  - WS で `op` メッセージとして送信、 サーバが受信して処理

### 4.4 WS メッセージプロトコル

#### 4.4.1 メッセージ型 (全列挙)

Server → Client:

| type | フィールド | 用途 |
|------|-----------|------|
| `hello`        | `plugin, version: number, irHash: string, sessionId: string` | 接続確立直後、 server 情報通知 |
| `ir`           | `root: IRNode, hash: string` | IR ツリー全体 (初回 + 構造変化時) |
| `binding`      | `path: string, value: unknown` | 単一 binding 値の更新 |
| `bindingBatch` | `changes: Array<{ path, value }>` | 複数 binding 値の一括更新 (高頻度向け) |
| `ack`          | `id: string, ok: bool, error?: string` | client `op` への応答 |
| `log`          | `level: 'info'\|'warn'\|'error', message: string, ts: number` | server 側からのログ通知 |
| `pong`         | `t: number` | heartbeat 応答 |
| `error`        | `code: string, message: string, fatal: bool` | プロトコルレベルのエラー (fatal なら接続切断予告) |

Client → Server:

| type | フィールド | 用途 |
|------|-----------|------|
| `hello`        | `token?: string, lastSessionId?: string` | 接続初回 (再接続なら lastSessionId 指定) |
| `subscribe`    | `paths: string[]` | 興味のある binding path (server の push 対象を絞る) |
| `unsubscribe`  | `paths: string[]` | 購読解除 |
| `set`          | `path: string, value: unknown` | widget からの値変更 |
| `op`           | `id: string, op: string, args?: unknown` | アクション発火 (id は client UUID) |
| `ping`         | `t: number` | heartbeat |

#### 4.4.2 接続ライフサイクル

```
┌────────────────────────────────────────────────────────────────────┐
│ 1. Client: WS connect to ws://host:5170/<plugin>/ws?token=<jwt>    │
│ 2. Server: verify token (Cernere middleware) — 失敗なら close 4001 │
│ 3. Client → Server: { type: 'hello', token, lastSessionId? }       │
│ 4. Server → Client: { type: 'hello', plugin, version, irHash, sessionId } │
│ 5. Server → Client: { type: 'ir', root, hash }                     │
│ 6. Client → Server: { type: 'subscribe', paths: [...] }            │
│ 7. (定常) binding / set / op の往復                                 │
│ 8. (定常) 15 秒ごとに ping / pong                                   │
│ 9. 切断時: Server は session を 5 分保持、 再接続を待機              │
└────────────────────────────────────────────────────────────────────┘
```

#### 4.4.3 Heartbeat

- **Client**: 15 秒ごとに `{ type: 'ping', t: <epoch_ms> }` を送信
- **Server**: 即座に `{ type: 'pong', t }` で応答
- **Server**: 30 秒間 ping を受信しない接続は close (code 4005)
- **Client**: 30 秒間 pong を受信しない場合は自発的に close → reconnect 開始

#### 4.4.4 Reconnect

- Exponential backoff: 1s, 2s, 4s, 8s, 16s, **30s で頭打ち**
- 再接続成功時に `hello` で `lastSessionId` を渡し、 server が 5 分以内なら同一 session 復元
- session 復元成功時: server は `ir` を送らず、 client は subscribe を再発行するだけで良い
- session 復元失敗時: server は新規 sessionId を発行 + `ir` を再送

#### 4.4.5 エラー処理

WS Close code (4xxx カスタム):

| code | 意味 | client の対応 |
|------|------|-------------|
| 4001 | 認証失敗 (token 不正) | 再認証要求、 reconnect 抑止 |
| 4002 | token 期限切れ | refresh token で再取得 → 再接続 |
| 4003 | plugin not found | reconnect しない、 ユーザに通知 |
| 4004 | protocol version mismatch | client / server の version 差異を表示 |
| 4005 | heartbeat timeout | 通常 reconnect |
| 4009 | server shutdown | exponential backoff で reconnect |

`op` の業務エラーは WS を維持したまま `ack { ok: false, error: '...' }` で返す。

#### 4.4.6 Binary message (canvas 等)

ArrayBuffer ベース。 最初 1 byte が tag、 続く ヘッダ + ペイロード:

| tag | name | header | payload |
|-----|------|--------|---------|
| `0x01` | canvas pixel push | `pathLen(uint16) + path(utf8) + width(uint32) + height(uint32) + format(uint8)` | pixel data (rgba8 / rgba32f) |
| `0x02` | streaming frame | `pathLen + path + frameId(uint32) + ts(uint64)` | encoded frame (画像 / 圧縮済み) |
| `0x10` | client → server input event (high freq) | `pathLen + path + eventKind(uint8)` | event data |

JSON message と binary message は同じ WebSocket connection で混在送信可能 (RFC 6455 の opcode で区別)。

#### 4.4.7 実装場所

- Server 側: `tools/ergo/src/core/ws/protocol.ts` で型定義 + 既存 `server.ts` の hub を拡張
- Web client: `tools/ergo/src/web/ws-client.ts` (新設) で reconnect / heartbeat / binary 分岐を集約
- C++ client: `ergo/include/ergo/ui_native/ir_client.h` で同等機能を実装 (§5.2 参照)

### 4.5 IR の TypeScript 型定義置場 (案)

`tools/ergo/src/core/ir/types.ts` に zod schema + TS 型を集中管理。
plugin は `definePlugin({ ir: (state) => ({...}) })` のように IR generator を export する。

### 4.6 kind 詳細仕様 (zod 定義)

#### 4.6.1 共通 Node base

```typescript
// tools/ergo/src/core/ir/types.ts
import { z } from 'zod';

const IRBindingsMap = z.record(z.string());           // field -> ir_path
const IRActionMap   = z.record(z.object({
  op:   z.string(),
  args: z.record(z.unknown()).optional()
}));

const IRNodeBase = z.object({
  kind:     z.string(),
  id:       z.string().optional(),  // 未指定なら parent path + index で自動生成
  props:    z.record(z.unknown()).optional(),
  bindings: IRBindingsMap.optional(),
  actions:  IRActionMap.optional(),
  children: z.lazy(() => z.array(IRNode)).optional()
});

export type IRNode = z.infer<typeof IRNodeBase>;
```

#### 4.6.2 レイアウト系

| kind | props | bindings | actions |
|------|-------|----------|---------|
| `panel`  | `{ title?: string, collapsed?: bool, layout: 'col'\|'row' = 'col', padding?: number }` | `collapsed?`, `title?` | `collapse?` |
| `row`    | `{ gap?: number, align?: 'start'\|'center'\|'end'\|'stretch' }` | — | — |
| `col`    | `{ gap?: number, align?: 'start'\|'center'\|'end'\|'stretch' }` | — | — |
| `tab`    | `{ tabs: Array<{ id, label, icon? }>, active?: string }` | `active?` | `change?` |
| `split`  | `{ orientation: 'h'\|'v', ratio: number = 0.5, minPx?: number }` | `ratio?` | `resize?` |
| `scroll` | `{ direction: 'y'\|'x'\|'both' = 'y' }` | — | — |

#### 4.6.3 表示系

| kind | props | bindings | actions |
|------|-------|----------|---------|
| `text`    | `{ value: string, size: 'xs'\|'sm'\|'md'\|'lg'\|'xl' = 'md', weight: 'normal'\|'bold' = 'normal', align?, color? }` | `value?`, `color?` | — |
| `label`   | `{ for: id, value: string }` | `value?` | — |
| `icon`    | `{ name: string, size?: number }` | `name?` | — |
| `image`   | `{ src: string, fit: 'contain'\|'cover'\|'fill' = 'contain' }` | `src?` | — |
| `badge`   | `{ value: string\|number, variant: 'neutral'\|'info'\|'warn'\|'error' = 'neutral' }` | `value?`, `variant?` | — |
| `divider` | `{ orientation?: 'h'\|'v' = 'h' }` | — | — |

#### 4.6.4 入力系

| kind | props | bindings | actions |
|------|-------|----------|---------|
| `input`    | `{ label?, placeholder?, type: 'text'\|'number'\|'password' = 'text', disabled?: bool }` | `value` (必須), `disabled?` | `change?`, `submit?` |
| `textarea` | `{ label?, placeholder?, rows: number = 4, disabled?: bool }` | `value` (必須), `disabled?` | `change?` |
| `slider`   | `{ label?, min: number, max: number, step: number = 1, unit?: string }` | `value` (必須), `disabled?` | `change?` |
| `toggle`   | `{ label?, disabled?: bool }` | `value` (bool, 必須) | `change?` |
| `select`   | `{ label?, options: Array<{ value: string, label: string }>, disabled?: bool }` | `value` (必須) | `change?` |
| `button`   | `{ label: string, variant: 'primary'\|'secondary'\|'danger' = 'secondary', disabled?: bool, icon? }` | `disabled?` | `click` (推奨) |
| `color`    | `{ label?, format: 'hex'\|'rgba' = 'hex' }` | `value` (必須) | `change?` |

#### 4.6.5 構造系

| kind | props | bindings | actions |
|------|-------|----------|---------|
| `tree`  | `{ selectable: bool = true, multi: bool = false, draggable?: bool }` | `nodes` (必須: `Array<{id,label,children?,expanded?}>`), `selected?`, `expanded?` | `select?`, `toggle?`, `rename?`, `move?` |
| `table` | `{ columns: Array<{ id, label, width? }>, sortable?: bool }` | `rows` (必須), `sort?` | `sort?`, `rowClick?` |
| `list`  | `{ itemHeight?: number }` | `items` (必須), `selected?` | `select?`, `itemClick?` |

#### 4.6.6 描画系 (特殊)

| kind | props | bindings | actions | 備考 |
|------|-------|----------|---------|------|
| `canvas`   | `{ width: number, height: number, pixelFormat: 'rgba8'\|'rgba32f' = 'rgba8', mode: 'static'\|'streaming' = 'static' }` | `data` (必須、 binary push 経路) | `click?`, `drag?`, `wheel?` | binary WS message で送る |
| `graph`    | `{ pannable: bool = true, zoomable: bool = true, nodeKinds: Array<{kind,label,inputs,outputs}> }` | `nodes` (必須), `edges` (必須), `selected?` | `addNode?`, `removeNode?`, `connect?`, `disconnect?`, `move?` | render_pipeline plugin で使用 |
| `chart`    | `{ chartType: 'line'\|'bar'\|'scatter', xAxis: AxisSpec, yAxis: AxisSpec }` | `series` (必須: `Array<{name, points}>`) | — | — |
| `timeline` | `{ height?: number, trackLabels?: string[], timeUnit: 'ms'\|'us' = 'ms' }` | `events` (必須: `Array<{track,t,dur,name,color?}>`) | `select?`, `zoom?` | profile plugin で使用 |

#### 4.6.7 IR サンプル (particle plugin)

```jsonc
{
  "version": 1,
  "plugin": "particle",
  "root": {
    "kind": "split",
    "props": { "orientation": "h", "ratio": 0.4 },
    "children": [
      {
        "kind": "panel",
        "props": { "title": "Particle Config", "layout": "col", "padding": 12 },
        "children": [
          {
            "kind": "slider",
            "id": "lifetime",
            "props": { "label": "Lifetime", "min": 0, "max": 10, "step": 0.1, "unit": "s" },
            "bindings": { "value": "particle.config.lifetime" },
            "actions": { "change": { "op": "particle.config.update" } }
          },
          {
            "kind": "color",
            "id": "tint",
            "props": { "label": "Tint", "format": "hex" },
            "bindings": { "value": "particle.config.tint" }
          },
          {
            "kind": "row",
            "children": [
              { "kind": "button", "props": { "label": "Spawn", "variant": "primary" }, "actions": { "click": { "op": "particle.spawn" } } },
              { "kind": "button", "props": { "label": "Clear", "variant": "danger" }, "actions": { "click": { "op": "particle.clear" } } }
            ]
          }
        ]
      },
      {
        "kind": "canvas",
        "id": "preview",
        "props": { "width": 480, "height": 360, "pixelFormat": "rgba8", "mode": "streaming" },
        "bindings": { "data": "particle.preview" },
        "actions": { "click": { "op": "particle.preview.click" } }
      }
    ]
  }
}
```

#### 4.6.8 拡張ルール

- 新 kind 追加時は zod schema を `types.ts` に追加し、 Web `<IRRenderer>` と
  `ergo_ui_native` 双方に対応コンポーネントを実装するまで PR を merge しない
- props は **後方互換** を保つ (新規 field は optional + default 値必須)
- kind の rename / 削除は major version bump (IR の `version: 1` → `2`)

## 5. 2 系統 Renderer

### 5.1 Web Renderer (外部ブラウザ)

- スタック: **React + Foundation UI** ([[project_foundation_ui]]) + Vite
- 場所: `tools/ergo/src/web/` (新設、 plugin 横断の共通レンダラ)
- エントリ: `<IRRenderer ws={..} pluginId={..} />`
- 各 kind に React コンポーネント (`<IRSlider>`, `<IRPanel>`, ...)
- WS 接続は単一の WebSocket client (Re-connect / heartbeat 内蔵)
- `.foundation-form` 角丸 + padding を Foundation UI ガイドに従う ([[feedback_memoria_foundation_input]])

### 5.2 In-proc Renderer (Ergo 側完結)

- スタック: **`ergo_ui_native` 単独** (Ergo の C++ モジュール、 Pictor は無変更)
- 採用理由: Dear ImGui / RmlUi を選ばず自作 (decision-metrics 評価表は §10 参照)
  - Ergo 側に primitives を集約することで、 Pictor / AdventureCube / KuzuSurvivors 等
    複数アプリで同じ in-proc UI が再利用できる
  - Pictor リポと依存関係が完全に分離されるため、 並行作業 / リリースサイクルが独立

#### ergo_ui_native の責務 (Ergo 側、 全機能を内包)

```
ergo/include/ergo/ui_native/
   ├─ ir_client.h        — Ergo :5170 と HTTP/WS 接続
   ├─ ir_renderer.h      — IR ツリー → primitives 呼び出し列の翻訳
   ├─ binding_store.h    — binding path → 現在値、 WS 同期
   ├─ action_dispatch.h  — 入力 → action WS 送信
   ├─ widget.h           — Widget 基底 (id, layout, draw, hit-test) — Ergo 側に定義
   ├─ primitives.h       — text / rect / line / image / icon (Pictor 描画 API ラッパ)
   ├─ layout.h           — flex-like layout engine (panel/row/col/split)
   ├─ input.h            — mouse/keyboard イベント分配 (ergo_input から取得)
   └─ state.h            — immediate-mode 風 widget state ストア (id → state)
```

#### Pictor 描画 API の利用方針

- `primitives.h` は **Pictor の既存描画 API (text draw / 2D rect / image blit 等) を opt-in で呼ぶ
  thin wrapper**。 Pictor は ergo_ui_native の存在を知らないし、 ergo_ui_native のために何も追加しない
- Pictor が提供しないプリミティブ (例: SDF text、 small immediate-mode rect batch、
  独自フォントアトラス) は **Ergo 側で軽量 helper を内製** する
  (Vulkan サブパス / 専用シェーダ / フォントアトラス管理は ergo_ui_native 内で完結)
- フォント / アイコン assets は `tools/ergo/assets/ui/` 配下で管理。 Pictor 側 [[project_pictor_visus]]
  の asset 流儀は参考にするが、 ergo_ui_native は独立した asset roster を持つ

#### 統合 API (アプリ側)

- アプリの main loop に組み込む形:

  ```cpp
  // Pictor demo / AdventureCube / KuzuSurvivors の main 等から
  ergo::ui_native::Renderer ui;
  ui.connect("http://localhost:5170");
  ui.subscribe("particle");   // 表示する plugin id
  ui.subscribe("profile");

  while (running) {
      pictor::Frame frame = renderer.begin_frame();
      // ... ゲーム描画 ...
      ui.draw(frame);          // ← in-proc UI を 2D overlay として描画
      renderer.end_frame(frame);
  }
  ```

- 表示する plugin ID は CLI / config / API いずれでも指定可能
- 接続失敗時 / link なし時は完全 no-op。 アプリ本体は影響を受けない

#### 5.2.x C++ API 仕様

##### Widget 基底 (`ergo/include/ergo/ui_native/widget.h`)

```cpp
namespace ergo::ui_native {

struct Rect  { float x, y, w, h; };
struct Color { float r, g, b, a; };
struct Size  { float w, h; };

struct WidgetId {
    uint64_t hash;
    constexpr WidgetId(std::string_view label, int seed = 0) noexcept;
    constexpr bool operator==(const WidgetId&) const = default;
};

enum class WidgetState : uint8_t {
    Idle, Hovered, Active, Focused, Disabled
};

struct LayoutHint {
    enum class Sizing { Fixed, Grow, Hug } w = Sizing::Hug, h = Sizing::Hug;
    float        fixed_w = 0.f, fixed_h = 0.f;
    float        grow_w = 1.f, grow_h = 1.f;
    float        gap = 4.f;
};

// 全 widget はこの interface を実装 (kind ごとに 1 つ)
class IWidget {
public:
    virtual ~IWidget() = default;
    virtual WidgetId      id() const = 0;
    virtual Size          measure(const LayoutContext&) = 0;     // 1st pass
    virtual void          place(const Rect& assigned) = 0;       // 2nd pass
    virtual void          draw(IRenderBackend&) = 0;
    virtual bool          hit_test(float x, float y) const = 0;
    virtual EventResponse on_event(const InputEvent&) = 0;
};

} // namespace
```

##### Primitives (`ergo/include/ergo/ui_native/primitives.h`)

```cpp
namespace ergo::ui_native {

// 描画 backend 抽象 — Pictor は無変更で、 既存 API を呼ぶだけのアダプタを書く
class IRenderBackend {
public:
    virtual void draw_rect(const Rect&, const Color&) = 0;
    virtual void draw_rect_rounded(const Rect&, const Color&, float radius) = 0;
    virtual void draw_text(const Rect&, std::string_view, const Color&,
                            float size_px, TextAlign = TextAlign::Left) = 0;
    virtual void draw_line(float x0, float y0, float x1, float y1,
                            const Color&, float thickness = 1.f) = 0;
    virtual void draw_image(const Rect&, ImageHandle) = 0;
    virtual void scissor_push(const Rect&) = 0;
    virtual void scissor_pop() = 0;
    virtual void clear(const Color&) = 0;
    virtual ~IRenderBackend() = default;
};

// Pictor 描画 API の thin adapter (Pictor は無変更、 ここだけ Pictor を呼ぶ)
class PictorRenderBackend final : public IRenderBackend {
public:
    explicit PictorRenderBackend(pictor::Frame& frame, FontAtlas& atlas);
    // ↑ Pictor の generic な draw_quad / draw_textured_quad しか必要なら、
    //   text は Ergo 内 FontAtlas で SDF レンダリングして draw_textured_quad に投げる
};

// Pictor が generic な draw API を提供する前提。
// 提供がない場合は Ergo 側に NullBackend + 独自 Vulkan サブパスを用意し、
// アプリの Vulkan command buffer に直接書き込むパスを別途用意 (将来)。

} // namespace
```

##### Layout (`ergo/include/ergo/ui_native/layout.h`)

```cpp
namespace ergo::ui_native {

class Layout {
public:
    // IR ツリーを flatten し、 IWidget のリストを生成
    void build(const ir::Node& root, WidgetFactory& factory);

    // 2-pass layout (Hug / Grow / Fixed の flex-like 実装)
    void compute(const Rect& root_rect);

    // 描画 (順序保証あり、 同一深度は IR 順)
    void draw(IRenderBackend&);

    // ヒットテスト (深い方優先)
    IWidget* hit(float x, float y);

private:
    std::vector<std::unique_ptr<IWidget>> widgets_;
    std::vector<int>                       z_order_;
};

} // namespace
```

##### State store (`ergo/include/ergo/ui_native/state.h`)

```cpp
namespace ergo::ui_native {

class StateStore {
public:
    // Immediate-mode 風: id ベースで widget 内部状態を引く
    template <typename T>
    T& get_or_default(WidgetId id, T default_value = {});

    // フォーカス管理
    void  set_focused(WidgetId);
    bool  is_focused(WidgetId) const;

    // フレーム終了時に未参照 id の state を GC
    void  end_frame();

private:
    struct Entry { std::any value; uint32_t last_seen_frame; };
    std::unordered_map<WidgetId, Entry, WidgetIdHasher> map_;
    WidgetId                                            focused_{};
    uint32_t                                            current_frame_ = 0;
};

} // namespace
```

##### IR client (`ergo/include/ergo/ui_native/ir_client.h`)

```cpp
namespace ergo::ui_native {

class IRClient {
public:
    IRClient();
    ~IRClient();

    // 接続
    void connect(std::string_view base_url, std::string_view token = {});
    void subscribe(std::string_view plugin_id);
    void unsubscribe(std::string_view plugin_id);
    void disconnect();

    // 受信した IR ツリー (plugin_id -> tree)
    const ir::Node* current_tree(std::string_view plugin_id) const;

    // binding store とのブリッジ
    BindingStore& bindings();

    // op 送信
    std::string send_op(std::string_view plugin_id, std::string_view op,
                        const ir::Args& args = {});  // 戻り値: client-side op id

    // 接続状態
    enum class Status { Disconnected, Connecting, Connected, Reconnecting };
    Status status() const;

    // コールバック
    using OnIRChanged = std::function<void(std::string_view plugin_id)>;
    using OnAck       = std::function<void(std::string_view op_id, bool ok,
                                           std::string_view error)>;
    void on_ir_changed(OnIRChanged);
    void on_ack(OnAck);

private:
    // 内部: WS thread + JSON / binary 分岐 + reconnect / heartbeat
};

} // namespace
```

##### Renderer (main loop integration)

```cpp
namespace ergo::ui_native {

class Renderer {
public:
    Renderer();
    void connect(std::string_view base_url, std::string_view token = {});
    void subscribe(std::string_view plugin_id);

    // アプリの per-frame 呼び出し
    void update(const InputState& input);    // 入力 → event dispatch / set / op 送信
    void draw(IRenderBackend& backend);      // 1 frame 描画

    // 接続状態
    IRClient::Status status() const;

private:
    IRClient      client_;
    Layout        layout_;
    StateStore    state_;
    BindingStore& bindings_; // client_.bindings() の参照
};

} // namespace
```

##### スレッドモデル

- `IRClient` の WS は **専用 background thread** で実行 (lock-free queue で main へ転送)
- `Renderer::update()` / `draw()` は **main thread (描画 thread)** で呼ぶ
- `BindingStore` は read-many-write-rare、 `shared_mutex` で守る
- `StateStore` は main thread 限定

##### エラー時の挙動

- `connect()` が失敗 → `status() == Disconnected`、 `Renderer::draw()` は no-op
- WS 切断 → background thread が exponential backoff で reconnect 試行 (§4.4.4)
- IR が未受信の plugin → 該当 plugin の描画はスキップ (他 plugin は通常描画)

##### ビルド / リンク方針

- `ergo_ui_native` は `add_library(ergo_ui_native)` で公開
- アプリ側は opt-in:
  ```cmake
  target_link_libraries(myapp PRIVATE ergo_ui_native)
  ```
- link しない場合、 アプリは ergo_ui_native の存在を一切意識しない
- 第三者依存: WebSocket client (cpp-httplib + 自前 WS or `uWebSockets`)、 JSON (`nlohmann::json`)、 SDF font (stb_truetype) は Ergo 側で vendoring

### 5.3 等価性保証

「同じ IR を Web と Pictor 内で同じセマンティクスに描く」 を保証するため:

- IR の **公式テストハーネス** を Phase 1 で整備 (snapshot test ベース)
- 各 kind ごとに Web / native 双方で同一の binding 動作を確認する integration test を用意
- Web 側を golden、 native 側を follower とし、 動作差分は Web に合わせる

## 6. Custos 機能の Ergo Plugin 化

旧 Custos 全機能を **`apps` / `capture` / `input` の 3 plugin** に再構成し、 Custos リポは obsolete。

### 6.1 apps plugin (`tools/ergo/src/plugins/apps/`)

#### 6.1.1 apps.json schema (zod、 旧 Custos `src/config/apps-config.ts` を 1:1 移植)

```typescript
import { z } from 'zod';

const CmdSpec = z.object({
  workingDir: z.string(),
  cmd:        z.string(),
  args:       z.array(z.string()).default([]),
  env:        z.record(z.string()).optional(),
  timeoutSec: z.number().positive().optional()
});

const CaptureSpec = z.object({
  type:        z.enum(['window', 'fullscreen', 'android', 'ergo_custos']),
  windowTitle: z.string().optional(),
  fps:         z.number().int().positive().default(30),
  preset:      z.string().default('ultrafast'),
  intervalSec: z.number().positive().default(1.0)
});

const ErgoCustosSpec = z.object({
  host: z.string().default('127.0.0.1'),
  port: z.number().int().positive().default(5198)
});

const InputButton = z.object({
  id:     z.string(),
  label:  z.string(),
  key:    z.string().optional(),
  action: z.string().optional()
}).refine(b => b.key || b.action, { message: 'key or action required' });

const InputSpec = z.object({
  buttons:        z.array(InputButton).default([]),
  allowKeyboard:  z.boolean().default(true),
  allowMouse:     z.boolean().default(true)
});

const LogsSpec = z.object({
  stdout: z.boolean().default(true),
  stderr: z.boolean().default(true),
  files:  z.array(z.string()).default([])
});

export const AppConfig = z.object({
  id:          z.string().regex(/^[a-z0-9-]+$/),
  name:        z.string(),
  description: z.string().optional(),
  target:      z.enum(['desktop', 'android']),
  build:       CmdSpec.optional(),
  run:         CmdSpec,
  test:        CmdSpec.optional(),
  capture:     CaptureSpec,
  ergoCustos:  ErgoCustosSpec.optional(),
  input:       InputSpec,
  logs:        LogsSpec
});

export type AppConfig = z.infer<typeof AppConfig>;
```

#### 6.1.2 REST

| Method | Path | Body | Response |
|--------|------|------|----------|
| `GET`  | `/apps/api/apps` | — | `{ apps: Array<AppSummary> }` |
| `GET`  | `/apps/api/apps/:id` | — | `AppDetail` (config + status + currentOpId) |
| `POST` | `/apps/api/apps/:id/build` | — | `{ opId }` |
| `POST` | `/apps/api/apps/:id/run`   | — | `{ opId }` |
| `POST` | `/apps/api/apps/:id/test`  | — | `{ opId }` |
| `POST` | `/apps/api/apps/:id/kill`  | — | `{ ok: true }` |
| `GET`  | `/apps/api/apps/:id/opLog/:opId` | — | `{ lines: string[], finished: bool }` (HTTP polling fallback) |
| `POST` | `/apps/api/apps/:id/reload` | — | `{ ok: true }` (apps.json hot-reload) |

```typescript
type AppSummary = {
  id: string;
  name: string;
  description?: string;
  target: 'desktop' | 'android';
  status: 'idle' | 'building' | 'running' | 'testing' | 'crashed';
  lastOpId?: string;
};

type AppDetail = AppSummary & {
  config: AppConfig;
  currentOpId?: string;
  pid?: number;
};
```

#### 6.1.3 WS `/apps/ws`

```typescript
// Client → Server
type C2S =
  | { type: 'subscribe',   appIds: string[] }
  | { type: 'unsubscribe', appIds: string[] };

// Server → Client
type S2C =
  | { type: 'opStart', appId: string, opId: string, kind: 'build'|'run'|'test', ts: number }
  | { type: 'log',     appId: string, opId: string, stream: 'stdout'|'stderr', line: string, ts: number }
  | { type: 'status',  appId: string, status: AppSummary['status'], pid?: number, ts: number }
  | { type: 'opEnd',   appId: string, opId: string, ok: boolean, code: number,
                       durationMs: number, ts: number };
```

#### 6.1.4 runner 実装

- `tools/ergo/src/plugins/apps/runner.ts`
- `child_process.spawn` ベース、 stdout / stderr を `readline` で行単位パース
- EventEmitter で `log` / `status` / `opEnd` イベント発火
- 同一 app の build → run → test は順次 (並列禁止)、 別 app は並列可
- 強制 kill は SIGKILL (Windows は `process.kill(pid, 'SIGKILL')` → 内部で `taskkill /T /F`)
- timeout 超過時は自動 kill + `opEnd { ok: false, code: -1 }`

#### 6.1.5 認証

- Cernere middleware を全 endpoint + WS に共通適用
- WS は `?token=<jwt>` を Hono 側で verify、 失敗時は close 4001

### 6.2 capture plugin (`tools/ergo/src/plugins/capture/`)

#### 6.2.1 REST

| Method | Path | Body | Response |
|--------|------|------|----------|
| `POST` | `/capture/api/rtc/offer` | `{ appId, sdp, sessionId? }` | `{ sessionId, sdp }` (answer) |
| `POST` | `/capture/api/rtc/close` | `{ sessionId }` | `{ ok: true }` |
| `GET`  | `/capture/api/sessions` | — | `{ sessions: Array<SessionSummary> }` |
| `GET`  | `/capture/api/diagnostic` | — | `{ recent: SessionDiagnostic[10] }` |
| `POST` | `/capture/api/frame/:appId` | (binary) | 単発 PNG 取得 fallback |

```typescript
type SessionSummary = {
  sessionId: string;
  appId: string;
  strategy: 'ergo_custos' | 'ffmpeg' | 'android';
  startedAt: number;
  frames: number;
  errors: number;
};

type SessionDiagnostic = SessionSummary & {
  endedAt?: number;
  trace: Array<{ ts: number, event: string, detail?: unknown }>;
};
```

#### 6.2.2 WS `/capture/ws`

```typescript
type C2S =
  | { type: 'subscribe',   appIds: string[] };

type S2C =
  | { type: 'sessionStart', sessionId: string, appId: string, strategy: string }
  | { type: 'frame',        sessionId: string, ts: number, frameId: number }  // metadata only
  | { type: 'error',        sessionId: string, error: string }
  | { type: 'sessionEnd',   sessionId: string, reason: 'client'|'app-exit'|'timeout'|'error' };
```

実フレームバイナリは WebRTC で配信 (`POST /capture/api/rtc/offer` で確立した PeerConnection)。
RTC 不可な環境のため、 WS の binary message 経路 (tag `0x02`、 §4.4.6) も用意。

#### 6.2.3 Strategy interface

```typescript
// tools/ergo/src/plugins/capture/strategy.ts

export interface CaptureStrategy {
  readonly id: 'ergo_custos' | 'ffmpeg' | 'android';

  canHandle(app: AppConfig): boolean;

  open(app: AppConfig, opts: {
    sessionId: string;
    sdp?: string;
  }): Promise<{
    sdp?: string;
    frames: AsyncIterable<Frame>;  // metadata stream (binary は別経路)
  }>;

  close(sessionId: string): Promise<void>;

  diagnostic(sessionId: string): SessionDiagnostic | null;
}

// 優先順 (上から試す)
export const strategies: CaptureStrategy[] = [
  new ErgoCustosCaptureStrategy(),  // app に ergoCustos が設定されていれば最優先
  new FfmpegCaptureStrategy(),      // desktop window/fullscreen
  new AndroidCaptureStrategy()      // android screenrecord
];

export function pickStrategy(app: AppConfig): CaptureStrategy {
  for (const s of strategies) if (s.canHandle(app)) return s;
  throw new Error(`no capture strategy for app ${app.id}`);
}
```

#### 6.2.4 ffmpeg / werift 統合

- werift `RTCPeerConnection` を server-side で常駐させ、 SDP exchange を REST で実施
- ffmpeg は subprocess、 `-f gdigrab` / `-f x11grab` / `-f avfoundation` を OS で分岐
- RTP UDP を werift の UDP listener に向けて投げる (旧 Custos 同様)
- 1 PC = 1 ffmpeg pipeline、 capture session の終了で SIGTERM

### 6.3 input plugin (`tools/ergo/src/plugins/input/`)

#### 6.3.1 REST

| Method | Path | Body | Response |
|--------|------|------|----------|
| `POST` | `/input/api/key`    | `{ appId, code: number, down: boolean }` | `{ strategy }` |
| `POST` | `/input/api/mouse`  | `{ appId, x?, y?, dx?, dy?, button?, down? }` | `{ strategy }` |
| `POST` | `/input/api/text`   | `{ appId, text: string }` | `{ strategy }` |
| `POST` | `/input/api/button` | `{ appId, buttonId: string }` | `{ strategy, action }` |
| `POST` | `/input/api/wheel`  | `{ appId, delta: number, axis?: 'y'\|'x' }` | `{ strategy }` |

`buttonId` は apps.json の `input.buttons[].id` を参照、 server が key / action に解決して `sendKey` / 該当 op を実行。

#### 6.3.2 Strategy interface

```typescript
// tools/ergo/src/plugins/input/strategy.ts

export interface InputStrategy {
  readonly id: 'ergo_custos' | 'nut-js' | 'adb';

  canHandle(app: AppConfig): boolean;

  sendKey(app: AppConfig, code: number, down: boolean): Promise<void>;
  sendMouse(app: AppConfig, opts: {
    x?: number; y?: number; dx?: number; dy?: number;
    button?: 'left'|'right'|'middle'; down?: boolean;
  }): Promise<void>;
  sendText(app: AppConfig, text: string): Promise<void>;
  sendWheel(app: AppConfig, delta: number, axis?: 'y'|'x'): Promise<void>;
}

// 優先順
export const strategies: InputStrategy[] = [
  new ErgoCustosInputStrategy(),  // 最優先 (app に ergoCustos 設定があれば)
  new NutJsInputStrategy(),       // desktop fallback
  new AdbInputStrategy()          // android target
];

export function pickStrategy(app: AppConfig): InputStrategy {
  for (const s of strategies) if (s.canHandle(app)) return s;
  throw new Error(`no input strategy for app ${app.id}`);
}
```

#### 6.3.3 nut-js / adb の optional dependency 化

- `nut-js` は `optionalDependencies` に移し、 ergo_custos がある環境では install 不要に
- `adb` は android target が apps.json にある場合のみ必要、 PATH 解決 + 起動チェック
- すべて欠ける場合: `canHandle` が false、 input plugin が 503 を返す

### 6.4 認証 (3 plugin 共通)

- Cernere middleware を `tools/ergo/src/core/auth/cernere.ts` に集約
- REST: `Authorization: Bearer <jwt>` ヘッダ検証 (5 分キャッシュ)
- WS: `?token=<jwt>` クエリ、 接続時に検証 → 失敗で close 4001
- Dev bypass: `ERGO_OPEN=1` 環境変数
- token 規約: per-user × per-project token ([[feedback_cernere_auth_only_endpoints]] / [[feedback_secret_per_user_memory_only]])

### 6.5 Custos リポの去就

- LUDIARS/Custos リポは **obsolete** に明記 (README に Ergo 統合済みの notice 追加)
- ブランチ・履歴は保全。 新規コミット禁止
- apps.json / public/ の HTML は Phase 2 完了時に Ergo へ完全移行済みの想定
- [[project_excubitor]] と同じ obsolete 移行を踏襲

## 7. ergo_custos の進化

### 7.1 現状 (spec/module/custos.md)

- HTTP/1.0 only、 endpoints: `/health` `GET /screenshot` `POST /key`
- callback 設計 (set_screenshot_provider / set_key_handler)
- 認証なし、 127.0.0.1 想定

### 7.2 拡張範囲 (別セッション実装)

| 機能 | 詳細 |
|------|------|
| WebSocket 追加 | `/ws` で binary screenshot streaming + UI IR 双方向通信 |
| マウス inject | `POST /mouse`, `/wheel`, `/click` |
| Cernere 認証 | `Authorization: Bearer <project-token>` ヘッダ検証 |
| IR consumer | `pictor_ui` 連携用に IR を受信して描画呼び出しへ翻訳 (ergo_ui_native 経由) |
| バイナリエンコーディング | screenshot は PNG → 帯域次第で JPEG / raw / H.264 切替可能に |

### 7.3 C++ WebSocket 実装方針

- 自前 RFC 6455 subset 実装 を最優先 (現行 HTTP/1.0 と同様 依存ゼロ)
- 規模が大きくなる場合は `uWebSockets` 系の header-only ライブラリを vendoring 検討
- 第三者依存追加は CMakeLists 修正範囲が大きい場合のみ

### 7.4 後方互換

- 既存の HTTP `/screenshot` `/key` は維持 (Phase 2 完了まで Custos が叩く可能性)
- spec/module/custos.md を別 PR で更新 (本書とは別の PR)

### 7.5 WS 仕様 (RFC 6455 subset 自前実装)

#### 7.5.1 サポート範囲

| 機能 | 対応 | 備考 |
|------|------|------|
| Text frame (opcode 0x1) | ○ | JSON ペイロード |
| Binary frame (opcode 0x2) | ○ | tag + ペイロード |
| Continuation frame (0x0) | ○ | 通常は単一フレームで完結だが parser は対応 |
| Ping (0x9) / Pong (0xA) | ○ | heartbeat |
| Close (0x8) | ○ | code + reason |
| Masking (client→server) | ○ | RFC 必須 |
| Masking (server→client) | × | RFC 通り未マスク |
| Payload length 16-bit | ○ | 64KB まで通常、 16-bit 拡張で 64KB 超〜16MB |
| Payload length 64-bit | × | 16MB 超は close 1009 (Message Too Big) |
| Subprotocol / Extensions | × | `Sec-WebSocket-Protocol` / `permessage-deflate` 等は無視 |

#### 7.5.2 ハンドシェイク

Client → Server:
```
GET /ws HTTP/1.1
Host: localhost:5198
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: <base64 16 bytes>
Sec-WebSocket-Version: 13
Authorization: Bearer <project-token>           (Phase 2 以降)
```

Server → Client:
```
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: <SHA-1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11") base64>
```

#### 7.5.3 アプリケーションメッセージ (text frame、 JSON)

S → C:

| type | フィールド | 用途 |
|------|-----------|------|
| `hello` | `version: number, caps: string[]` | server 情報 (caps: `["screenshot","key","mouse","ir"]`) |
| `ir`    | `plugin: string, root: IRNode` | Ergo から受けた IR を pictor 側に中継 |
| `binding` | `plugin, path, value` | binding 更新中継 |
| `ack`   | `id: string, ok: boolean, error?: string` | client 要求への応答 |

C → S:

| type | フィールド | 用途 |
|------|-----------|------|
| `hello` | `client: 'ergo_ui_native'\|'custos'\|'ergo'` | client 識別 |
| `key` | `code: number, down: boolean` | キー injection (旧 `POST /key` の WS 版) |
| `mouse` | `x?, y?, dx?, dy?, button?, down?` | マウス injection |
| `screenshotReq` | `id: string, format: 'png'\|'raw_rgba8'` | 単発スクリーンショット要求 (応答は binary frame) |
| `irSubscribe`   | `plugin: string` | Ergo IR 中継の対象 plugin 追加 |
| `irUnsubscribe` | `plugin: string` | 対象 plugin 解除 |

#### 7.5.4 Binary frame format

| tag (1st byte) | name | format |
|---------------|------|--------|
| `0x01` | screenshot push | `width(u32 BE) + height(u32 BE) + format(u8: 0=png,1=raw_rgba8) + payload` |
| `0x02` | streaming frame | `frameId(u32) + ts(u64 BE) + payload` |
| `0x10` | input event burst | `n(u16) + n × { kind(u8) + body... }` |

#### 7.5.5 実装構造 (C++)

```cpp
// ergo/include/ergo/custos/ws_server.h (新設)
namespace ergo::custos {

enum class WsCloseCode : uint16_t {
    Normal              = 1000,
    GoingAway           = 1001,
    ProtocolError       = 1002,
    UnsupportedData     = 1003,
    NoStatus            = 1005,
    AbnormalClosure     = 1006,
    InvalidPayload      = 1007,
    PolicyViolation     = 1008,
    MessageTooBig       = 1009,
    InternalError       = 1011,
    Unauthorized        = 4001,    // 認証失敗 (Phase 2 以降)
    TokenExpired        = 4002
};

class WsConnection {
public:
    using OnText    = std::function<void(std::string_view)>;
    using OnBinary  = std::function<void(std::span<const std::byte>, uint8_t tag)>;
    using OnClosed  = std::function<void(WsCloseCode, std::string_view reason)>;

    void send_text(std::string_view utf8);
    void send_binary(std::span<const std::byte>, uint8_t tag);
    void send_ping(std::span<const std::byte> body = {});
    void close(WsCloseCode = WsCloseCode::Normal, std::string_view reason = {});

    void on_text(OnText);
    void on_binary(OnBinary);
    void on_closed(OnClosed);

private:
    Socket sock_;
    // 内部: RFC 6455 frame parser / serializer + 1 thread per connection
};

class WsServer {
public:
    explicit WsServer(uint16_t port, std::string_view bind_host = "127.0.0.1");
    void start();
    void stop();

    using OnConnect = std::function<void(std::shared_ptr<WsConnection>,
                                          const HandshakeInfo&)>;
    void on_connect(OnConnect);

private:
    // 内部: TCP accept loop → HTTP upgrade handshake → WsConnection 生成
};

struct HandshakeInfo {
    std::string                                  path;
    std::unordered_map<std::string, std::string> headers;
    std::optional<std::string>                   bearer_token;
};

} // namespace
```

#### 7.5.6 frame parser の境界

- 1 connection = 1 receive thread (blocking read)、 送信は `std::mutex` で直列化
- 単一 frame の payload 上限: 16 MB (RFC 64-bit 長は未対応、 超過は close 1009)
- continuation frame は内部 buffer に追記し、 FIN 受信時に on_text / on_binary を一括 dispatch
- masking key 4 byte は payload に XOR 適用 (SIMD なし、 small payload 想定で素朴ループ)

#### 7.5.7 vendoring vs 自前

- **自前実装** を既定 (600-800 行見込み、 依存ゼロを維持)
- 実装難航 / parser バグ多発時の fallback: `uWebSockets-cpp` を `third_party/uwebsockets/` に vendoring
- 完全 RFC 6455 準拠は非ゴール (text + binary + ping/pong + close + masking で必要十分)

#### 7.5.8 接続ライフサイクル

```
client                              server
  │ TCP connect                       │
  │──────────────────────────────────→│
  │ HTTP upgrade (Sec-WebSocket-Key)  │
  │──────────────────────────────────→│
  │           101 Switching Protocols │
  │←──────────────────────────────────│
  │ text: { type: 'hello', client }   │
  │──────────────────────────────────→│
  │           text: { type: 'hello',  │
  │                   version, caps } │
  │←──────────────────────────────────│
  │ (定常: key / mouse / screenshotReq) │
  │ ←─────── binary screenshot ─────── │
  │ ←─────── text: ir / binding ────── │
  │ ping / pong (15s 間隔)             │
  │ close 1000 / 4001 / etc.          │
  └────────────────────────────────────┘
```

## 8. Cernere 認証

### 8.1 Ergo core に middleware として実装

- 場所: `tools/ergo/src/core/auth/cernere.ts` (旧 Custos `src/auth/cernere-auth.ts` を移植)
- 5 分の verify 結果キャッシュ
- dev bypass: `ERGO_OPEN=1` (旧 `CUSTOS_OPEN=1` に相当)
- WS 接続時は `?token=...` を verify

### 8.2 トークン規約 [[feedback_secret_per_user_memory_only]]

- per-user × per-project token (`/api/auth/project-token`、 [[feedback_cernere_auth_only_endpoints]])
- process memory only、 永続化禁止
- 失効時は WS を切断 + 再接続を要求

### 8.3 plugin への適用

- 各 plugin が `requireAuth: true` (デフォルト) または `false` を宣言可能
- ローカル開発便宜のため `particle` / `variable` 等の既存 plugin は `requireAuth: false` も選択肢
  (Phase 2 完了時に全 plugin `true` 化を最終決定)

## 9. 移行手順

### Phase 1 — 基盤 (現セッション設計、 別セッション実装着手中)

- 1.1 spec/tool/ergo-redesign.md (本書) を merge
- 1.2 IR schema を zod + TS 型で確定 (`tools/ergo/src/core/ir/types.ts`)
- 1.3 React `<IRRenderer>` 雛形 + 最小 kind (panel/text/button) で動作確認
- 1.4 `ergo_ui_native` module skeleton 作成 (IR client + 最小翻訳 + text/rect/panel primitives + flex layout を一括内蔵)
  - Pictor リポは触らない。 Pictor の描画 API を opt-in で呼ぶ thin wrapper のみ用意
- 1.5 `ergo_custos` に WebSocket 追加 (binary 通信ベース確立)

### Phase 2 — Custos 移植

- 2.1 Ergo に `apps` plugin scaffold (REST + WS、 IR 対応で初手から書く)
- 2.2 runner.ts 移植 (child_process spawn / EventEmitter)
- 2.3 `capture` plugin (werift broker + Strategy pattern)
- 2.4 `input` plugin (Strategy pattern)
- 2.5 Cernere middleware 全 plugin 適用
- 2.6 LUDIARS/Custos リポ obsolete 化 (README notice + PR で archive 提案)

### Phase 3 — 既存 plugin の IR 化 (段階移植)

各 plugin を 1 PR ずつ、 IR 未対応 kind を必要に応じ追加しながら移植:

- 3.1 `variable` plugin → IR (tree + edit、 kind 追加: `tree`)
- 3.2 `profile` plugin → IR (canvas が大物、 kind 追加: `timeline`)
- 3.3 `particle` plugin → IR (slider 群 + preview、 kind 追加: `canvas` 拡張)
- 3.4 `render_pipeline` plugin → IR (NodeGraph、 kind 追加: `graph`)
- 3.5 `rive` plugin → IR (外部 SDK 連動 canvas)

各 plugin 移植時は **Vanilla JS と IR を一時並存** 可能にする (フラグ `?ir=1` で切替)。

### Phase 4 — 外部 plugin pack 対応

- kzs-web / ac-web に **migration guide** を提供 ([[feedback_ergo_editor_plugin_pack]])
- Vanilla HTML plugin の fallback 互換 shim を残す (即時 IR 化を強制しない)
- 各ホストリポ側で順次 IR 化

### 9.4 Phase ごとの成果物 / Acceptance criteria

#### Phase 1 (基盤)

成果物:
- `spec/tool/ergo-redesign.md` (本書) main 入り
- `tools/ergo/src/core/ir/types.ts` (IR 全 kind の zod schema)
- `tools/ergo/src/web/IRRenderer.tsx` (React renderer 雛形)
- `tools/ergo/src/core/ws/protocol.ts` (WS message 型)
- `ergo/include/ergo/ui_native/` + `ergo/src/ui_native/` (C++ skeleton)
- `ergo/include/ergo/custos/ws_server.h` + `ergo/src/custos/ws_server.cpp` (WS server 拡張)
- `tools/ergo/src/core/auth/cernere.ts` (middleware skeleton、 デフォルト bypass)

Acceptance:
- [ ] spec/tool/ergo-redesign.md が main にいる
- [ ] IR の最小 kind (`panel` / `text` / `button` / `slider`) が zod で定義され、 unit test pass
- [ ] React `<IRRenderer>` が `?ir=1` パラメータで起動した状態で、 server から push された IR ツリーを 4 種 kind で再帰描画できる
- [ ] WS heartbeat (15s ping / 30s timeout) と reconnect (1s→30s backoff) が integration test で確認
- [ ] `ergo_ui_native` が空アプリ (Pictor demo) と link して、 IR で送られた `panel/text/button` を画面に描画できる
- [ ] `ergo_custos` に WS が立ち、 `/ws` で IR `subscribe` → 受信できる
- [ ] Cernere middleware が opt 状態で、 `ERGO_OPEN=1` のとき token なしで通る
- [ ] **Pictor リポへの commit がゼロ** であることを最終チェック

#### Phase 2 (Custos 移植)

成果物:
- `tools/ergo/src/plugins/apps/` (REST + WS + runner)
- `tools/ergo/src/plugins/capture/` (WebRTC broker + Strategy)
- `tools/ergo/src/plugins/input/` (Strategy)
- LUDIARS/Custos リポ: README に obsolete notice 追加 PR

Acceptance:
- [ ] `apps` plugin が apps.json を読み、 build/run/test/kill が REST で動作
- [ ] WS で `opStart / log / status / opEnd` が時系列に配信される
- [ ] `capture` plugin が ffmpeg + ergo_custos の 2 経路で screenshot streaming できる
- [ ] WebRTC SDP offer/answer 交換が成功し、 PeerConnection が ICE-connected に遷移
- [ ] `input` plugin が key/mouse/text を送信、 strategy fallback (ergo_custos → nut-js → adb) が動作
- [ ] Cernere middleware が全 endpoint に強制適用、 未認証は 401 / WS close 4001
- [ ] Custos リポの README に「obsolete、 Ergo に統合済み、 新規コミット禁止」 notice を追加
- [ ] 旧 Custos の Web UI 機能パリティ (apps 操作 / log 表示 / screenshot 表示) を満たす

#### Phase 3 (各 plugin)

各 plugin で:
- [ ] `/api/plugins` レスポンスで `irReady: true` を返す
- [ ] `/<plugin>/ir` が IR JSON を返す
- [ ] WS bindings/actions が双方向同期 (set → server / binding push → client)
- [ ] `?ir=1` で React 描画、 `?ir=0` で Vanilla 描画の **並存** 確認
- [ ] `ergo_ui_native` でも同じ IR が描画される
- [ ] 既存機能の機能パリティ (snapshot test、 visual regression < 5px 差)
- [ ] 各 plugin の IR / ops 一覧を `spec/tool/ergo-redesign.md` 付録 A に追記

Plugin 別の追加 acceptance:
- 3.1 `variable`: tree + table、 actor/binding registry に live edit
- 3.2 `profile`: timeline kind 新規、 Chrome Trace event 描画
- 3.3 `particle`: slider 群 + streaming canvas、 60fps preview
- 3.4 `render_pipeline`: graph kind 新規、 NodeGraph save/load
- 3.5 `rive`: streaming canvas + inputs table

#### Phase 4 (外部 plugin pack)

成果物:
- `tools/ergo/docs/migration-guide-plugin-pack.md` (新設)
- kzs-web / ac-web リポに 1 plugin だけ IR 移植したサンプル PR

Acceptance:
- [ ] migration guide が plugin pack 作者の手で読める粒度 (IR generator export 方法 + 旧 Vanilla → 新 IR の対応表)
- [ ] kzs-web / ac-web の **1 plugin だけ** が IR 化されて Pictor 内で見える
- [ ] 残りの Vanilla HTML plugin は fallback shim で起動可能 (壊さない)
- [ ] ホストリポ側の追従 PR は各 game リポで個別に発行

### 9.5 テスト方針

#### 9.5.1 Unit tests (jest + vitest)

- IR schema validation (zod): kind ごとの props / bindings / actions 受理 / 拒否
- Layout engine の compute (input rect + IR → expected widget rects)
- WS message serialization / deserialization (text + binary)
- Cernere token verify の cache hit / miss / 期限切れ挙動
- WS frame parser (RFC 6455 subset、 各種 opcode / masking / continuation)

#### 9.5.2 Integration tests (Hono + supertest + ws library)

- WS connect → hello → IR push → subscribe → set → ack の往復
- WS heartbeat 維持と Server 側 timeout 切断
- Reconnect 時の sessionId 復元 / 新規発行の分岐
- `apps` plugin の build → run → kill ライフサイクル (子プロセスで `node -e 'setInterval(...)'` を使用)
- `capture` plugin の RTC offer/answer (werift をループバックで使用)
- `input` plugin の strategy fallback (ergo_custos を mock で再現)
- Cernere middleware の 401 / WS close 4001 path

#### 9.5.3 Visual regression (Phase 3 以降)

- ツール: **Playwright** (Web 側) + **自前 image diff** (native 側、 stb_image_write で PNG dump)
- 各 plugin の代表 UI 状態 (default / hover / focused / error) を golden snapshot
- React renderer 出力 と ergo_ui_native 出力 を **同じ golden に対して** 比較
  - 許容: per-pixel RGB delta < 5、 differing pixel ratio < 1%
  - 失敗時は diff 画像を artifact に出して PR review に貼る

#### 9.5.4 Equivalence tests (Web ↔ native)

- 同じ IR を Web React + ergo_ui_native の両方で並行描画
- binding 更新 → 双方で値が同期されているか (1 秒以内)
- action 発火 → 双方が同じ WS message を送っているか (順序保証含む)
- Web が golden、 native が follower、 差分は native 側を修正

#### 9.5.5 E2E (Phase 2 以降)

- apps.json 駆動の build/run/test を本物の AdventureCube / KuzuSurvivors で実行 (CI 上では skip flag)
- WebRTC screenshot を画像にして OCR で UI 要素を抽出 (assertion)
- input event injection が在席アプリに届くか (アプリ側のログ確認)

#### 9.5.6 CI 構成 (案)

```
.github/workflows/ergo-redesign.yml
   ├─ lint (eslint + prettier + clang-format)
   ├─ unit (vitest + ctest)
   ├─ integration (Hono + werift in docker)
   ├─ visual (Playwright in headless Chrome)
   └─ equivalence (Web + ergo_ui_native の docker image)
```

### 9.6 Migration shim (Vanilla JS と IR の並存)

#### 9.6.1 サーバ側分岐

`/api/plugins` レスポンスに `irReady: boolean` を追加:

```typescript
GET /api/plugins
→ 200 {
    plugins: [
      { id: "particle",       title: "Particles",       irReady: true,  legacyUrl: "/particle/"     },
      { id: "variable",       title: "Variables",       irReady: true,  legacyUrl: "/variable/"     },
      { id: "rive",           title: "Rive",            irReady: false, legacyUrl: "/rive/"         },
      { id: "spawn",          title: "Spawn (kzs)",     irReady: false, legacyUrl: "/spawn/"        },
    ]
  }
```

各 plugin は IR generator を export していれば `irReady: true`、 そうでなければ `false` (旧 Vanilla 互換 plugin pack)。

#### 9.6.2 Shell の描画モード

| URL パラメータ | 挙動 |
|----------------|------|
| `?ir=auto` (デフォルト) | plugin の `irReady` を見て分岐: true なら IR renderer、 false なら iframe で legacyUrl |
| `?ir=1` | 全 plugin を IR renderer で描画。 `irReady: false` の plugin は `<unsupported>` 表示 |
| `?ir=0` | 全 plugin を iframe で描画 (legacyUrl)。 旧運用との互換性 |

#### 9.6.3 段階移行のリリースルール

- ある plugin が IR 化された PR が main に入った時点で `irReady: true`
- 旧 Vanilla UI のソース (`tools/ergo/src/plugins/<id>/ui/`) は **次のマイナーリリース** まで残す
- 次のリリースで旧 Vanilla UI 削除 + `?ir=0` を deprecation warning
- すべての組み込み plugin が IR 化されたら `?ir=0` を削除 (`?ir=1` がデフォルト相当)

#### 9.6.4 外部 plugin pack の fallback shim

- 外部 plugin pack (kzs-web / ac-web 等) が IR generator を export しない場合:
  - 起動時に `irReady: false` でロード
  - shell は iframe で legacyUrl を埋め込み
  - ergo_ui_native では描画不可 (subscribe 時に `ack { ok: false, error: 'plugin not ir-ready' }`)
- IR 移植が完了したら export を追加するだけで `irReady: true` に切り替わる (server 再起動なし、 plugin hot-reload で OK)

#### 9.6.5 ergo_ui_native の対応

- ergo_ui_native は IR pull のみサポート (Vanilla HTML を解釈しない)
- 未 IR plugin を `subscribe` しようとした場合:
  - WS で `ack { ok: false, error: 'plugin not ir-ready' }` を返す
  - ergo_ui_native は該当 plugin を描画リストから外す (他 plugin の描画は継続)
- ergo_ui_native で見たい plugin は **必ず IR 化されている必要がある** (Phase 3 の優先順位は「Pictor 内で見たい順」)

## 10. 設計判断の根拠 (decision-metrics)

### 10.1 アプリ内描画レイヤー

| 案 | (1) AI 負荷 | (2) 工数 | (3) 解決度 | (4) 一致度 | 採用? |
|----|------------|---------|-----------|-----------|--------|
| A1. Dear ImGui (C++) | 2 | 中 (~600 行) | 5 (即 PoC) | 4 (ImGui 調で固定) | ◎ |
| **A3. Ergo 側 ergo_ui_native で自作 (Pictor 描画 API を opt-in で呼ぶ、 採用)** | **3** | **大 (primitives + layout 1500-2000 行)** | **4** | **5 (Pictor 無変更 + LUDIARS 流儀一致)** | **◎** |
| A2. RmlUi (HTML/CSS subset) | 3 | 大 (subset 学習 + 結局書き直し) | 3 | 3 | △ |
| A4. egui (Rust) | 4 | 大 (Rust 取込) | 4 | 2 (LUDIARS で初) | × |

**選定: A3**。 工数は ImGui より重いが、 `ergo_ui_native` を Ergo 側に集約することで:
1. Pictor リポは無変更 ([[feedback_pictor_no_upper_dep]] を完全維持、 Pictor 側の並行作業と衝突しない)
2. 複数アプリ (Pictor demo / AdventureCube / KuzuSurvivors) で同じ in-proc UI を再利用可能
3. リリースサイクルが Pictor から独立 (Ergo 単独でバージョン管理)

### 10.2 Custos の去就

| 案 | (1) AI 負荷 | (2) 工数 | (3) 解決度 | (4) 一致度 | 採用? |
|----|------------|---------|-----------|-----------|--------|
| **B1. 完全廃止 → Ergo の 3 plugin に移植 (採用)** | **3** | **大 (~2000 行)** | **5 (オーバーヘッド完全解消)** | **5** | **◎** |
| B2. Custos core を library 化、 Electron だけ廃止 | 2 | 中 | 4 | 4 | △ |
| B3. Custos 残し、 Ergo plugin から HTTP で叩く | 1 | 小 | 2 (overhead 残) | 1 | × |

### 10.3 IR 化の射程

| 案 | (1) AI 負荷 | (2) 工数 | (3) 解決度 | (4) 一致度 | 採用? |
|----|------------|---------|-----------|-----------|--------|
| **C2. 新規 IR、 既存は段階移植 (採用)** | **2** | **中 (plugin ごと 200-500 行)** | **4** | **5** | **◎** |
| C1. 全 plugin 一括 IR 化 | 4 | 大 | 5 | 5 | △ (並行セッション衝突リスク大) |
| C3. Pictor で見たい plugin だけ IR | 1 | 小 | 2 (作り直しゴール未達) | 2 | × |

## 11. 互換性 / リスク

| 項目 | 影響 | 緩和策 |
|------|------|--------|
| 既存 5 plugin の UI | Phase 3 で 1 つずつ移植、 並存期間あり | フラグ `?ir=1` で切替、 旧 UI は最後の plugin が完了するまで保持 |
| 外部 plugin pack (kzs-web / ac-web) 破壊 | Phase 4 で migration guide | Vanilla HTML fallback shim を残す |
| `ergo_ui_native` primitives 自作の工数 | Phase 1.4 で primitives 確立必須 | 最初は最小セット (panel/text/button/slider) のみ、 plugin 移植と同期して拡張 |
| **Pictor リポと衝突するリスク** | **設計上ゼロ** | Pictor リポへの変更は新規モジュール / 既存ファイル含め一切なし。 ergo_ui_native は Pictor の既存描画 API のみを呼ぶ thin wrapper として動作 |
| ergo_custos の WS 拡張 | C++ で RFC 6455 subset 実装 | 自前実装が現実的、 fallback で uWebSockets vendoring も検討 |
| werift 取込 | UDP RTP listener、 ffmpeg 外部プロセス依存 | 現行 Custos と同じ動作で移植、 別途最適化は範囲外 |
| 並行セッションとの衝突 | Phase 1 実装と本仕様書を別ブランチで進行中 | spec/ergo-redesign ブランチに分離、 実装ブランチが先に main 入りしてもよい |
| Cernere 認証強制化 | 開発初期は摩擦 | `ERGO_OPEN=1` の dev bypass、 plugin 単位の `requireAuth` |

## 12. 別セッション (実装側) への引き継ぎ

- **PR 分割**:
  - Phase 1 は 1 PR (IR schema + Web 雛形 + ergo_ui_native skeleton + ergo_custos WS) [[feedback_ai_pr_size]]
  - **Pictor リポへの変更は本シリーズでは発生しない** (Pictor 並行作業と衝突回避を最優先)
  - Phase 2 は 1 PR (3 plugin 移植 + Custos obsolete 化)
  - Phase 3 は plugin ごとに 1 PR (5 PR)
  - Phase 4 はホストリポ側 PR
- **IR 拡張ポイント**: Phase 3 の各 plugin 移植で IR に新 kind 追加。 zod schema を `tools/ergo/src/core/ir/types.ts` で central に更新
- **ergo_custos WS は ergo_custos 自体の PR で先行**。 Phase 1 と同期 (横断変更は 1 PR、 CLAUDE.md §「横断変更」)
- **本仕様書 (`spec/tool/ergo-redesign.md`) は Phase 完了ごとに更新**。 最終 Phase で `spec/tool/ergo.md` に統合 or 置換
- **Phase 1 完了時の動作確認**: `tools/ergo` を起動し、 `?ir=1` フラグで panel/text/button が Web に表示される + Pictor アプリで `--ergo-panel=demo` を渡すと同じ panel が in-proc 描画される

## 付録 A. 既存 plugin の IR 移植サンプル

Phase 3 で各 plugin を IR 化する際の **完成形イメージ**。
ここで挙げた `op` は WS で client → server に送る `{ type: 'op', op, args }` の `op` 値。

### A.1 variable plugin → IR

Actor ツリー + 変数 table の組み合わせ。

```jsonc
{
  "version": 1,
  "plugin": "variable",
  "root": {
    "kind": "split",
    "props": { "orientation": "h", "ratio": 0.35 },
    "children": [
      {
        "kind": "panel",
        "props": { "title": "Actors", "layout": "col" },
        "children": [{
          "kind": "tree",
          "id": "actorTree",
          "props": { "selectable": true, "multi": false, "draggable": false },
          "bindings": { "nodes": "variable.actors", "selected": "variable.selectedActor", "expanded": "variable.expanded" },
          "actions": {
            "select": { "op": "variable.selectActor" },
            "toggle": { "op": "variable.toggleExpanded" }
          }
        }]
      },
      {
        "kind": "panel",
        "props": { "title": "Variables", "layout": "col" },
        "children": [{
          "kind": "table",
          "id": "varTable",
          "props": {
            "columns": [
              { "id": "name",  "label": "Name", "width": 200 },
              { "id": "type",  "label": "Type", "width": 80 },
              { "id": "value", "label": "Value" }
            ],
            "sortable": true
          },
          "bindings": { "rows": "variable.bindings" },
          "actions": {
            "rowClick": { "op": "variable.editBinding" },
            "sort":     { "op": "variable.sort" }
          }
        }]
      }
    ]
  }
}
```

- **新規 kind**: なし (既存 `tree` / `table` で表現可)
- **ops**: `variable.selectActor`, `variable.toggleExpanded`, `variable.editBinding`, `variable.set` (binding 値変更時の implicit set), `variable.sort`

### A.2 profile plugin → IR

Chrome Trace Event タイムライン。

```jsonc
{
  "version": 1,
  "plugin": "profile",
  "root": {
    "kind": "col",
    "props": { "gap": 8 },
    "children": [
      {
        "kind": "row",
        "props": { "gap": 8 },
        "children": [
          {
            "kind": "select",
            "props": {
              "label": "Frame",
              "options": []
            },
            "bindings": { "value": "profile.frameIdx", "options": "profile.frameOptions" },
            "actions":  { "change": { "op": "profile.selectFrame" } }
          },
          { "kind": "button", "props": { "label": "Reload" }, "actions": { "click": { "op": "profile.reload" } } },
          { "kind": "button", "props": { "label": "Export Chrome Trace" }, "actions": { "click": { "op": "profile.exportTrace" } } }
        ]
      },
      {
        "kind": "timeline",
        "id": "trace",
        "props": { "height": 400, "timeUnit": "us", "trackLabels": ["Main", "Render", "Worker"] },
        "bindings": { "events": "profile.events", "viewport": "profile.viewport" },
        "actions": {
          "select": { "op": "profile.selectEvent" },
          "zoom":   { "op": "profile.zoom" }
        }
      },
      {
        "kind": "panel",
        "props": { "title": "Selected Event" },
        "children": [{
          "kind": "tree",
          "bindings": { "nodes": "profile.selectedDetails" }
        }]
      }
    ]
  }
}
```

- **新規 kind**: `timeline` (Phase 3.2 で追加)
- **ops**: `profile.reload`, `profile.exportTrace`, `profile.selectFrame`, `profile.selectEvent`, `profile.zoom`

### A.3 render_pipeline plugin → IR

NodeGraph editor。

```jsonc
{
  "version": 1,
  "plugin": "render_pipeline",
  "root": {
    "kind": "split",
    "props": { "orientation": "h", "ratio": 0.7 },
    "children": [
      {
        "kind": "graph",
        "id": "pipeline",
        "props": {
          "pannable": true,
          "zoomable": true,
          "nodeKinds": [
            { "kind": "pass",   "label": "Render Pass", "inputs": [{"name":"in","type":"attachment"}], "outputs": [{"name":"out","type":"attachment"}] },
            { "kind": "shader", "label": "Shader",      "inputs": [{"name":"src","type":"glsl"}],     "outputs": [{"name":"obj","type":"spirv"}] },
            { "kind": "attach", "label": "Attachment",  "inputs": [],                                  "outputs": [{"name":"out","type":"attachment"}] }
          ]
        },
        "bindings": {
          "nodes":    "pipeline.nodes",
          "edges":    "pipeline.edges",
          "selected": "pipeline.selected"
        },
        "actions": {
          "addNode":    { "op": "pipeline.addNode" },
          "removeNode": { "op": "pipeline.removeNode" },
          "connect":    { "op": "pipeline.connect" },
          "disconnect": { "op": "pipeline.disconnect" },
          "move":       { "op": "pipeline.move" }
        }
      },
      {
        "kind": "panel",
        "props": { "title": "Inspector" },
        "children": [
          { "kind": "tree", "bindings": { "nodes": "pipeline.inspector" } },
          { "kind": "row",
            "children": [
              { "kind": "button", "props": { "label": "Save",   "variant": "primary" },   "actions": { "click": { "op": "pipeline.save" } } },
              { "kind": "button", "props": { "label": "Reload", "variant": "secondary" }, "actions": { "click": { "op": "pipeline.reload" } } }
            ]
          }
        ]
      }
    ]
  }
}
```

- **新規 kind**: `graph` (Phase 3.4 で追加)
- **ops**: `pipeline.addNode`, `pipeline.removeNode`, `pipeline.connect`, `pipeline.disconnect`, `pipeline.move`, `pipeline.save`, `pipeline.reload`

### A.4 rive plugin → IR

Rive runtime preview + input editor。

```jsonc
{
  "version": 1,
  "plugin": "rive",
  "root": {
    "kind": "col",
    "props": { "gap": 8 },
    "children": [
      {
        "kind": "row",
        "props": { "gap": 8 },
        "children": [
          { "kind": "select", "props": { "label": "File",      "options": [] }, "bindings": { "value": "rive.file",      "options": "rive.fileOptions" },      "actions": { "change": { "op": "rive.loadFile" } } },
          { "kind": "select", "props": { "label": "Artboard",  "options": [] }, "bindings": { "value": "rive.artboard",  "options": "rive.artboardOptions" },  "actions": { "change": { "op": "rive.setArtboard" } } },
          { "kind": "select", "props": { "label": "Animation", "options": [] }, "bindings": { "value": "rive.animation", "options": "rive.animationOptions" }, "actions": { "change": { "op": "rive.setAnimation" } } },
          { "kind": "button", "props": { "label": "Play"  }, "actions": { "click": { "op": "rive.play"  } } },
          { "kind": "button", "props": { "label": "Pause" }, "actions": { "click": { "op": "rive.pause" } } }
        ]
      },
      {
        "kind": "canvas",
        "id": "rivePreview",
        "props": { "width": 640, "height": 480, "pixelFormat": "rgba8", "mode": "streaming" },
        "bindings": { "data": "rive.frame" },
        "actions":  { "click": { "op": "rive.previewClick" } }
      },
      {
        "kind": "panel",
        "props": { "title": "Inputs" },
        "children": [{
          "kind": "table",
          "props": { "columns": [
            { "id": "name",  "label": "Name",  "width": 200 },
            { "id": "type",  "label": "Type",  "width": 80 },
            { "id": "value", "label": "Value" }
          ] },
          "bindings": { "rows": "rive.inputs" },
          "actions":  { "rowClick": { "op": "rive.setInput" } }
        }]
      }
    ]
  }
}
```

- **新規 kind**: なし (`canvas streaming` + `table` で表現可)
- **ops**: `rive.loadFile`, `rive.setArtboard`, `rive.setAnimation`, `rive.play`, `rive.pause`, `rive.setInput`, `rive.previewClick`

### A.5 particle plugin → IR

§4.6.7 のサンプル参照。 補足 ops 一覧:

- **新規 kind**: なし
- **ops**: `particle.config.update`, `particle.spawn`, `particle.clear`, `particle.preview.click`, `particle.replace` (full state replace)

### A.6 新規 kind 投入スケジュール (Phase 3 内訳)

| Phase | Plugin | 新規 kind | 既存 kind 拡張 |
|-------|--------|----------|---------------|
| 3.1 | variable | — | `tree` (draggable), `table` (sortable) |
| 3.2 | profile  | `timeline` | `select` (動的 options) |
| 3.3 | particle | — | `canvas` (mode='streaming' のフレーム push 確立) |
| 3.4 | render_pipeline | `graph` | `tree` (inspector) |
| 3.5 | rive     | — | `canvas` (streaming), `table` (rowClick op) |

各 Phase の PR は **(a) zod schema 拡張 + (b) Web React 実装 + (c) ergo_ui_native 実装 + (d) plugin 側 IR generator + (e) visual regression golden 更新** を 1 PR にまとめる ([[feedback_ai_pr_size]])。

## 13. 参考 / 関連メモリ

- [[project_corpus]] — 宣言的レンダリング思想 (サービスが UI を JSON 宣言、 Corpus が描画) と同根
- [[project_foundation_ui]] — Web 側 React renderer のデザイン基盤
- [[project_pictor_visus]] — asset 宣言 / フォントの流儀を参考にするが、 ergo_ui_native は独立 asset roster を持つ
- [[feedback_pictor_no_upper_dep]] — Pictor は無変更、 GUI primitives は全て ergo_ui_native (Ergo 側) に内蔵
- [[feedback_cernere_auth_only_endpoints]] / [[feedback_secret_per_user_memory_only]] — Cernere 認証規約
- [[feedback_ergo_editor_plugin_pack]] — 外部 plugin pack 方針
- [[feedback_ai_pr_size]] — Phase 単位の 1 PR 集約
- [[feedback_concurrent_session_branch]] — 並行セッション衝突回避 (本書は spec/ergo-redesign ブランチで分離)
