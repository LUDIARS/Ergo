# ゲーム固有エディタを plugin pack として読み込む設定

## 目的

ゲーム固有のエディタ (例: KuzuSurvivors の spawn / skill、AdventureCube の
placer / terrain / acstage) を、`tools/ergo` の core/shell を fork せずに
読み込む。**一般則**: ゲーム Web エディタは `tools/ergo` を fork せず、
ホストリポ側の **plugin pack** + `ERGO_PLUGIN_DIR` で拡張する
(kzs-web は upstream を独自拡張する例外。詳細は
[`../tool/ergo.md`](../tool/ergo.md) 「外部プラグイン」)。

## 設定キー

| 環境変数 | 役割 |
|---|---|
| `ERGO_PLUGIN_DIR` | plugin pack ディレクトリ。OS のパス区切り (Windows は `;`、他は `:`) で複数指定可。各ディレクトリ直下のサブディレクトリで `index.js` または `index.ts` を持つものを動的 import し、`default` export (`PluginFactory`) を組み込み factory に併合する (`tools/ergo/src/core/external.ts`) |

- `ERGO_PLUGIN_DIR` 未設定なら外部プラグインは 0 件 (組み込みのみ)。
- 壊れた pack エントリ (`default` が関数でない / import 失敗) は **ログを出して
  スキップ** し、起動は止めない (`external.ts:43-56`)。
- 既定の解決順: `process.cwd()` 相対の絶対パス化 → `index.js` 優先・無ければ
  `index.ts` (tsx 経由で読む / `external.ts:36-39`)。

## plugin pack 側の作法 (ホストリポで用意するもの)

正本は [`../tool/ergo.md`](../tool/ergo.md) 「外部プラグイン」。要点のみ:

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

# 例 (PowerShell): KuzuSurvivors の plugin pack を読み込んで起動
$env:ERGO_PLUGIN_DIR = "E:\Document\Ars\KuzuSurvivors\tools\kzs-web\plugins"
npm run dev

# 複数 pack を渡す場合 (Windows は ; 区切り)
$env:ERGO_PLUGIN_DIR = "C:\a\plugins;C:\b\plugins"
npm run dev
```

Electron 起動 (`npm start`) でも `electron/main.cjs` が tsx の ESM ローダを
register するため、`.ts` のままの外部プラグインを読み込める
(`electron/main.cjs:58-62`、`external.ts` を dist から呼ぶ)。

## 既存の plugin pack (参照)

- KuzuSurvivors: `tools/kzs-web/plugins/{spawn,skill}`
- AdventureCube: `tools/ac-web/plugins/{placer,terrain,acstage}`

(いずれも Ergo リポには存在せず、各ホストリポ側。`registry.ts` / `external.ts`
のコメントが正本)

## 注意点

- ゲーム固有プラグインを `tools/ergo/src/plugins/` や `registry.ts` に**入れない**
  (組み込みは汎用エディタのみ)。Ergo を fork せず外側で足すのが原則。
- 起動時の cwd はホスト側になり得るため、pack 内のパス解決は必ず
  `import.meta.url` 基準にする (cwd 相対だと外部ロード時に壊れる)。
