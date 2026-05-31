# 組み込みプラグインにホスト資産を見せる設定

## 目的

組み込み (汎用) プラグインのうち、`render_pipeline` と `visus` は **ホスト側
(Pictor / ゲームプロジェクト) の資産ファイル** を読み書きする。資産の場所を
環境変数で指定するためのガイド。指定しない場合は cwd 相対の既定 (兄弟リポ
推定) にフォールバックする。

## 設定キー

| 環境変数 | 対象プラグイン | 役割 / 既定の解決 |
|---|---|---|
| `ERGO_PICTOR_PROFILE_DIR` | `render_pipeline` | Pictor の `*.profile.json` を置くディレクトリ。未設定時は cwd から `../../../Pictor/profiles` を試し、無ければ cwd の `profiles/` (`tools/ergo/src/plugins/render_pipeline/profile_store.ts:30-39`) |
| `VISUS_PROJECT_ROOTS` | `visus` | 描画定義 (`*.visus.json` 等) を走査するプロジェクトルート。`;` 区切りで複数指定可。未設定時は `process.cwd()` 1 件 (`tools/ergo/src/plugins/visus/index.ts:46-51`) |

- どちらも相対指定は cwd 基準で絶対パス化される。
- `render_pipeline` の保存 (`POST /render_pipeline/api/profile`) は
  `ERGO_PICTOR_PROFILE_DIR` が解決したディレクトリへ書き込む。
- `visus` は指定ルート配下に閉じ込めるパス検証を行い、`..` でのエスケープを
  弾く (`index.ts` `resolveSafe`)。

## 手順

```bash
cd tools/ergo
npm install

# 例 (PowerShell): Pictor の profile を編集対象にしてヘッドレス起動
$env:ERGO_PICTOR_PROFILE_DIR = "E:\Document\Ars\Pictor\profiles"
$env:VISUS_PROJECT_ROOTS     = "E:\Document\Ars\KuzuSurvivors"
npm run dev
```

起動後:

- `render_pipeline`: `http://localhost:5170/render_pipeline/` で
  `*.profile.json` のグラフ編集 (エンドポイント詳細は
  [`../tool/render_pipeline.md`](../tool/render_pipeline.md))
- `visus`: `http://localhost:5170/visus/` で描画定義の一覧・編集

## 注意点

- 兄弟リポ推定 (`../../../Pictor/profiles`) は **Ergo と Pictor を横並びで
  clone している前提**。配置が違う場合は `ERGO_PICTOR_PROFILE_DIR` を明示する。
- これらの env は組み込みプラグイン用で、外部 (ゲーム固有) プラグインの
  ロードに使う `ERGO_PLUGIN_DIR` ([external-plugins.md](external-plugins.md))
  とは別物。
