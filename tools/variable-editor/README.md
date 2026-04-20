# Variable Editor

任意のアプリ変数を **C++ 側で `BIND_VAR(...)` するだけ** で自動的に Web UI に
表示し、ライブで編集できるツール。

```
[browser UI] ──WS── [variable-editor server] ──WS── [adventurecube.exe (engine)]
                       (registry, SoT)                 ergo_bind: BIND_VAR("name", var, ...)
```

ホスト側 (engine) はアウトバウンドで接続するだけなので、Win32 サーバ実装なしで
Windows でも動作する。

## 場所

`ergo` リポジトリ ブランチ `module/bind` の `tools/variable-editor/`。
ergo_bind モジュール (C++) と同じブランチで管理されている。

## 起動

```bash
# standalone (ergo を直接 clone)
cd <ergo>/tools/variable-editor

# あるいは host 側 worktree 経由 (例: AdventureCube)
cd <host>/external/ergo/bind/tools/variable-editor

npm install
npm run dev    # tsx watch でホットリロード
# または
npm start
```

デフォルト `http://localhost:5174/`。`PORT=NNNN npm start` で変更可。

## 構成

| パス | 内容 |
|---|---|
| `src/server.ts`   | Hono + ws の HTTP/WS サーバ。各 engine 接続 ↔ UI 接続を仲介し、registry を保持 |
| `src/protocol.ts` | wire protocol 型定義 (engine ↔ server, ui ↔ server) |
| `public/index.html` | ブラウザ UI (型ごとに editor を自動生成) |

## ロール

クライアントは接続時に `hello` で自分のロールを宣言する:

- **engine** — アプリ。`bind` / `value` / `unbind` を送信、`set` を受信
- **ui**     — ブラウザ。`set` を送信、`registry` / `value` を受信

## サポート型

| kind | UI |
|---|---|
| `bool`              | チェックボックス |
| `int32` / `int64`   | スライダー (meta.min<max のとき) または数値入力 |
| `float` / `double`  | スライダー (同上) |
| `string`            | テキスト入力 |
| `color`             | カラーピッカー (RGBA) |
| `vec3`              | 3 つの数値入力 |

メタデータは `min`, `max`, `step`, `read_only`, `category`, `unit`。
`category` で同じ app 内のグルーピングが行われる。

## REST

- `GET /api/health` — `{ok, vars, clients}`
- `GET /api/vars`   — 現時点の登録変数一覧

## エンジン側実装

`ergo_bind` モジュールを利用 (Ergo, ブランチ `module/bind`)。AdventureCube の
統合例は `AdventureCube/src/main.cpp` を参照。
