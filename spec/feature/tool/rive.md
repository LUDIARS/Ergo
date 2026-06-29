# rive plugin — Rive Player + 構造可視化

## 概要

Ergo 統合開発者ツール (`tools/ergo`) の組み込みプラグインの 1 つ。
`.riv` ファイルをドラッグ&ドロップ (またはファイルダイアログ) で受け取り、
browser canvas 上で再生 + 構造を可視化する。

KS / AC など Pictor の Rive runtime を使うホストで「この .riv は何を含むか」
「どの state machine input がどう動くか」 を運用中にチェックできるのが狙い。

## 主機能

- **再生**: Rive WebGL ランタイム (`@rive-app/canvas-advanced` v2.30) で
  artboard.draw → renderer.flush の per-frame ループ。 fit/alignment 切替可
- **構造可視化**:
  - artboard 一覧 (名前 / 寸法 / state machine 数 / animation 数)
  - 選択 artboard の animation 一覧 (各 duration 秒)
  - state machine inputs (bool / number / trigger 全部 GUI 操作可)
  - 簡易 timeline (animation 再生時は duration 内のカーソル / SM は 10s wrap)
- **WebSocket hub**: 1 クライアントが publish したメタ情報を他クライアントに
  broadcast。 将来 KS 側 (Vulkan ベンチ) から接続して同一 riv の構造を
  共有する用途を想定

## 設置場所

- 実装: `tools/ergo/src/plugins/rive/`
  - `index.ts` — plugin definition (Hono routes + WS broadcaster)
  - `ui/index.html` — UI 本体
  - `ui/styles.css` — スタイル
  - `ui/app.js` — Rive runtime ロード + 再生 + 可視化ロジック
- 登録: `tools/ergo/src/core/registry.ts` の `PLUGIN_FACTORIES` に追加

## URL

| URL                                  | 役割                                          |
|--------------------------------------|----------------------------------------------|
| `http://localhost:5170/rive/`        | Player UI                                    |
| `http://localhost:5170/rive/api/meta`| 現在 publish 中のメタ情報                       |
| `http://localhost:5170/rive/api/health` | プラグインヘルス                            |
| `ws://localhost:5170/rive/ws`        | meta 同期 (publish / state 受信)                |

## プロトコル (WS)

クライアント → サーバ:

```json
{ "op": "publish", "meta": { "name": "santa.riv", "size": 129909,
    "artboards": [{
        "name": "Santa", "width": 780, "height": 650,
        "stateMachineNames": ["State Machine 1"],
        "animationNames": ["idle"],
        "animationDurations": [0.667]
    }]
}}
```

```json
{ "op": "ping" }
```

サーバ → クライアント:

```json
{ "op": "meta", "meta": <RivMeta|null>, "clients": <number> }
```

```json
{ "op": "ack" }
```

## 依存

- ランタイム: ブラウザ側のみ。 `@rive-app/canvas-advanced@2.30.0` を unpkg から
  動的 import。 サーバ側に npm 依存は無い (Hono + ws は既存)。
- オフライン環境では unpkg がブロックされるため CDN を差し替えるか、
  将来 npm dep に切り替える必要あり

## 既知の制限

- artboard 単位での timeline スクラブは未実装 (Play/Pause + speed のみ)。
  Rive runtime の API でランダムアクセス seek は限定的 (LinearAnimationInstance
  に setTime はあるが Stable API 化されてない)
- state machine の遷移グラフ可視化は未対応 (Rive runtime が state list を
  公開しないため、 inputs + active state name 表示までが現実的)
- ファイルは 1 個ずつ。 複数 .riv の比較は別タブで開く運用

## テスト

- `tools/ergo` を起動 (`npm run dev`) → `http://localhost:5170/rive/` で UI
- KS の `data/rive/sample.riv` / `santa.riv` / `sample3.riv` を読ませて:
  - 各 artboard の寸法・SM・anim 数が出ること
  - SM input 操作で再生が変化すること (bool/trigger)
  - speed スライダで再生速度が変わること
- `/rive/api/health` が `{ ok: true, version: 1 }` を返すこと
