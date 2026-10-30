# 開発者ツール `tools/ergo` を起動するための設定

## 目的

Ergo の共有プラグインホスト型 Web 開発者ツール (`tools/ergo`) を起動する。
1 Node プロセス / 1 ポートで、組み込み (汎用) プラグインを iframe シェルに
束ねて表示し、エンジンクライアント (`ergo_bind` / `ergo_particle_link` /
`ergo_scene_link` / `ergo_log_live`) は同じポートの WebSocket に接続する。
全体仕様は [`../feature/tool/ergo.md`](../feature/tool/ergo.md)。初めてなら
[quickstart.md](quickstart.md) が clone から「ブラウザで値を変えて反映」までを通す。

## 前提

- Node `>= 22.12.0` (`tools/ergo/package.json` の `engines.node`)
- `git submodule update --init -- lib/lapilli` で共有 CLI 起動ライブラリを取得する。
- 依存はバックエンドが `hono` + `@hono/node-server` + `ws`、起動形態に
  `electron` + `tsx` + `typescript` (`tools/ergo/package.json`)

## 起動モードと npm スクリプト

`tools/ergo/package.json` の `scripts` に対応 (正本):

| コマンド | モード | 内容 |
|---|---|---|
| `npm start` (= `npm run app`) | **既定: Electron スタンドアロン** | `tsc` でビルド後 `electron .` で `BrowserWindow` を開き、同プロセスで Node サーバを boot |
| `npm run serve` | ヘッドレス (1 回) | `tsx src/main.ts`。ウィンドウを開かず Node サーバのみ。ブラウザから接続 |
| `npm run dev`   | ヘッドレス (watch) | `tsx watch src/main.ts`。ファイル変更で再起動 |
| `npm run build` | ビルドのみ | `tsc -p tsconfig.json` (`npm start` が内部で実行) |
| `npm test` | テスト | `node --import tsx --test "tests/**/*.test.ts"`。`tests/{core,helpers,plugins/<id>}/` にあり、一時ディレクトリとポート 0 を使う hermetic なテスト |
| `npm run typecheck` | 型検査のみ | `tsc -p tsconfig.test.json` (`src/` と `tests/` を出力なしで検査)。CI のジョブ `tools-ergo` も実行する ([ci.md](ci.md)) |

ヘッドレス (`serve` / `dev`) は CI・Docker・リモート接続・ブラウザ devtools
主体のケース向け ([`../feature/tool/ergo.md`](../feature/tool/ergo.md) 「起動モード」)。

## 設定キー

| 環境変数 | 既定 | 役割 |
|---|---|---|
| `PORT` | `5170` | サーバの listen ポート。Electron (`electron/main.cjs:17`) / ヘッドレス (`src/main.ts:10`) のどちらも参照 |
| `ERGO_HOST` | `127.0.0.1` | bind するアドレス (ヘッドレスのみ、`src/main.ts:11`)。既定は LAN に出さない。Electron は読まない。LAN に出してもブラウザからは IP アドレスか `ERGO_HOST` の名前で開く (他の名前は `Host` 検査で 403。[`../feature/tool/ergo.md`](../feature/tool/ergo.md)「接続元の制限」) |

`PORT` を変えるとエンジンクライアントの接続先 (`ws://127.0.0.1:<PORT>/<id>/ws`)
も変わる点に注意 (後述)。プラグイン固有の変数 (`ERGO_PLUGIN_DIR` / `ERGO_PICTOR_PROFILE_DIR` /
`VISUS_PROJECT_ROOTS` / `ERGO_VFX_BACKEND` など) の一覧は
[config-reference.md](config-reference.md) の「環境変数 (tools/ergo)」。

## ポートと URL

既定ポート **5170**。主な URL (詳細は [`../feature/tool/ergo.md`](../feature/tool/ergo.md)
「ポートと URL」):

| URL | 役割 |
|---|---|
| `http://localhost:5170/`            | シェル UI (プラグイン選択) |
| `http://localhost:5170/api/plugins` | 登録プラグイン一覧 |
| `http://localhost:5170/api/health`  | 全プラグイン集約ヘルス |
| `http://localhost:5170/<id>/`       | プラグイン `<id>` の UI |
| `ws://localhost:5170/<id>/ws`       | プラグイン `<id>` の WebSocket |

## 組み込み (汎用) プラグイン

`src/core/registry.ts` の `PLUGIN_FACTORIES` に列挙されたものだけが組み込み
(どのホストでも使う汎用エディタ)。一覧・対応する C++ モジュール・エンジン側の
供給元・仕様書は [`../feature/tool/ergo.md`](../feature/tool/ergo.md) の
「プラグイン一覧」にある (正本は `registry.ts`。ctest `test_module_catalog` が
両者の一致を検査する)。ゲーム固有エディタ (placer / spawn / skill 等) はここには
無く、外部ロードする ([external-plugins.md](external-plugins.md))。

`render_pipeline` / `visus` はホスト側の資産 (Pictor profile / 描画定義) を
参照するため追加の env 設定が要る場合がある
([plugin-data-roots.md](plugin-data-roots.md))。

## 手順

```bash
cd tools/ergo
npm install

# 既定: Electron スタンドアロン
npm start

# もしくはヘッドレス (ブラウザで http://localhost:5170/ を開く)
npm run dev

# ポートを変える場合 (PowerShell)
$env:PORT = "5180"; npm run dev
```

## 注意点

- `dist/` と `node_modules/` は `tools/ergo/.gitignore` で追跡外。`npm start` は
  毎回 `tsc` で `dist/` を作り直す。
- Electron 経路では `electron/main.cjs` が `dist/core/*.js` を動的 import する
  ため、`npm start` (= build → electron) の順を崩さない。`serve` / `dev` は
  `tsx` で `src/` を直接実行するのでビルド不要。
- Electron アプリは preload 経由で `window.ergo.electron` を露出し、選択中
  プラグインに応じてウィンドウタイトルを `ergo — <title>` に更新する
  (`electron/main.cjs:48-52`)。
