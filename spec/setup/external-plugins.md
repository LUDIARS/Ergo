# ゲーム固有エディタを plugin pack として読み込む設定

## 目的

ゲーム固有のエディタ (例: PrivateGame の spawn / skill、AdventureCube の
placer / terrain / acstage) を、`tools/ergo` の core/shell を fork せずに
読み込む。**一般則**: ゲーム Web エディタは `tools/ergo` を fork せず、
ホストリポ側の **plugin pack** + `ERGO_PLUGIN_DIR` で拡張する
(game-web は upstream を独自拡張する例外。詳細は
[`../feature/tool/ergo.md`](../feature/tool/ergo.md) 「外部プラグイン」)。

## 設定キー

| 環境変数 | 役割 |
|---|---|
| `ERGO_PLUGIN_DIR` | plugin pack ディレクトリ。OS のパス区切り (Windows は `;`、他は `:`) で複数指定可。各ディレクトリ直下のサブディレクトリで `index.js` または `index.ts` を持つものを動的 import し、`default` export (`PluginFactory`) を組み込み factory に併合する (`tools/ergo/src/core/external.ts`) |

- `ERGO_PLUGIN_DIR` 未設定なら外部プラグインは 0 件 (組み込みのみ)。読めない
  ディレクトリは警告を出して飛ばす。
- 既定の解決順: `process.cwd()` 相対の絶対パス化 → pack の中はエントリ名の順 →
  `index.js` 優先・無ければ `index.ts` (tsx 経由で読む)。`index.js` が優先されるのは
  どの起動経路 (`serve` / `dev` / Electron) でも同じ。

### プラグイン id と壊れたエントリの扱い

規則の正本は [`../feature/tool/ergo.md`](../feature/tool/ergo.md) の
「プラグイン id と読み込みの規則」。要点:

- id は `/^[a-z0-9][a-z0-9_-]*$/` に合い、`api` / `shell` 以外であること。`title` は
  文字列、`description` / `icon` / `staticRoot` はあれば文字列、`routes` / `onUpgrade` /
  `health` はあれば関数
- 壊れた外部エントリは **ログを出してスキップ** し、起動は止めない: import の失敗、
  `default` (または `factory`) の関数 export が無い、ファクトリや `routes()` の例外、
  形の違反、id の重複
- id が重なったら先に名乗ったものが勝つ。組み込みが先、外部は `ERGO_PLUGIN_DIR` の
  順、pack の中はエントリ名の順。組み込みの id (`particle` など) は外部から上書きできない
- ファクトリ・形・重複・`routes()` で外したものは `/api/health` の
  `rejected: [{id, source, ok:false, err}]` に出て、全体の `ok` が false になる
  (import 失敗と export 無しはログだけ)

## plugin pack 側の作法 (ホストリポで用意するもの)

正本は [`../feature/tool/ergo.md`](../feature/tool/ergo.md) 「外部プラグイン」。要点のみ:

- `<host>/tools/<name>-web/plugins/<id>/index.ts` (+ `schema.ts` / `ui/` 等) を
  置く。各 `index.ts` は `Plugin` を返す **ファクトリ関数を default export**。
- プラグイン契約は Ergo の `core/plugin.ts` を `plugins/_contract.ts` に
  コピーし `import type` で使う (ランタイム結合は無い)。
- 静的アセットの `staticRoot` は cwd 相対ではなく
  `fileURLToPath(new URL("./ui", import.meta.url))` で自分の絶対パスにする。
- `plugins/package.json` に pack が import する依存 (`hono` / `ws`) を宣言し、
  ホストのランチャ (`editor.bat` 等) が pack で `npm install` してから
  `ERGO_PLUGIN_DIR` を渡してツールを起動する。

## 手順

```bash
cd tools/ergo
npm install

# 例 (PowerShell): PrivateGame の plugin pack を読み込んで起動
$env:ERGO_PLUGIN_DIR = "E:\Document\Ars\PrivateGame\tools\game-web\plugins"
npm run dev

# 複数 pack を渡す場合 (Windows は ; 区切り)
$env:ERGO_PLUGIN_DIR = "C:\a\plugins;C:\b\plugins"
npm run dev
```

Electron 起動 (`npm start`) でも `electron/main.cjs` が tsx の ESM ローダを
register するため、`.ts` のままの外部プラグインを読み込める
(`electron/main.cjs`、`external.ts` を dist から呼ぶ)。

## 既存の plugin pack (参照)

- PrivateGame: `tools/game-web/plugins/{spawn,skill}`
- AdventureCube: `tools/ac-web/plugins/{placer,terrain,acstage}`

(いずれも Ergo リポには存在せず、各ホストリポ側。`registry.ts` / `external.ts`
のコメントが正本)

## 注意点

- ゲーム固有プラグインを `tools/ergo/src/plugins/` や `registry.ts` に**入れない**
  (組み込みは汎用エディタのみ)。Ergo を fork せず外側で足すのが原則。
- 起動時の cwd はホスト側になり得るため、pack 内のパス解決は必ず
  `import.meta.url` 基準にする (cwd 相対だと外部ロード時に壊れる)。
