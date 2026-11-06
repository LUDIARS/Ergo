# rive plugin — Rive Player + 構造可視化

## 概要

Ergo 統合開発者ツール (`ergo_studio`、 `tools/studio`) の組み込みプラグインの 1 つ。
開いている `.riv` ファイルのメタ情報 (アートボード / state machine / アニメーション) を
中継し、 構造を可視化する。 サーバは publish されたメタ情報を全接続へ配るだけで、
再生そのものはアプリのウィンドウ側の仕事。

PrivateGame / AC など Pictor の Rive runtime を使うホストで「この .riv は何を含むか」
「どの state machine input がどう動くか」 を運用中にチェックできるのが狙い。

## 主機能

- **構造可視化** (publish されたメタ情報から):
  - artboard 一覧 (名前 / 寸法 / state machine 数 / animation 数)
  - 選択 artboard の animation 一覧 (各 duration 秒)
  - state machine inputs (bool / number / trigger 全部 GUI 操作可)
  - 簡易 timeline (animation 再生時は duration 内のカーソル / SM は 10s wrap)
- **WebSocket hub**: 1 クライアントが publish したメタ情報を他クライアントに
  broadcast。 将来 PrivateGame 側 (Vulkan ベンチ) から接続して同一 riv の構造を
  共有する用途を想定。 アプリのウィンドウ側は in-process API (`publish()` / `meta()`)
  で同じハブに載せる

## 設置場所

- 実装: `tools/studio/plugins/rive/`
  - `rive_plugin.{h,cpp}` — プラグイン本体 (HTTP ルート + WS broadcaster + in-process API)
- 登録: `tools/studio/builtin_plugins.cpp`
- 旧 `tools/ergo/src/plugins/rive/` (TypeScript + ブラウザ UI) は 2026-11-06 に削除

## URL

| URL                                  | 役割                                          |
|--------------------------------------|----------------------------------------------|
| (Studio の `rive` タブ)              | UI                                           |
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

- サーバ側に外部依存は無い (`ergo_studio` の net / host 層のみ)。 旧版の
  ブラウザ側 Rive WebGL ランタイム (`@rive-app/canvas-advanced`、 unpkg からの
  動的 import) は 2026-11-06 の削除で無くなった

## 既知の制限

- state machine の遷移グラフ可視化は未対応 (メタ情報に state list が無い。
  inputs + active state name 表示までが現実的)
- ファイルは 1 個ずつ (`meta` は最後に publish されたもの)

## テスト

- ctest `tests/studio/test_studio_files.cpp` (`RivePlugin` の publish → `meta` の配信、
  `GET /rive/api/meta`)
- 手動: `ergo_studio` を起動 → `rive` タブで PrivateGame の `data/rive/sample.riv` /
  `santa.riv` / `sample3.riv` を読ませて、 各 artboard の寸法・SM・anim 数が出ること
- `/rive/api/health` が `ok: true` を返すこと
