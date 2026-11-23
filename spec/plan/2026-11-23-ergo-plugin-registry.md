# Ergo: プラグイン置き場 (レジストリ) とダウンロード

依頼 (2026-11-23, neco): 「プラグインをダウンロードするためのプラグイン置き場を用意」。
上位方針: [キュレーションとオーケストレーション](2026-11-23-ergo-curation-and-data-tables.md) — キュレーションの「選ぶ元」がこの置き場。

## 今あるもの (実装済み、neco「今もプラグイン置き場の実装があるはず」で確認 2026-11-23)

- **取得の仕組み (ST-INV-08、`spec/feature/tool/studio.md` §ST-INV-08)**: `tools/studio/plugins/studio/plugin_installer.*` /
  `plugin_source.*` / `plugin_ledger.*`。旧 tools/ergo の plugin-installer.ts の C++ 移植。
  - 取得元は `owner/repo:subpath[@rev]` (例 `LUDIARS/AdventureCube:tools/ac-web/plugins@main`)。owner は許可リスト
    (`ERGO_PLUGIN_OWNERS`、既定 `LUDIARS`) だけ。subpath の `..`・絶対パス・`\` は拒否。
  - preview: rev を 40 桁 sha に解決 → subpath だけ sparse fetch → 中身 (コミット・プラグイン名・置き換える版) を見せる。
  - install: preview した staging をそのまま移し (再取得しない)、各プラグインを Studio SDK (`plugin_abi.h`) で CMake ビルド、
    台帳 `<studio home>/plugins/ledger.json` に sha を固定。読み込みは次の起動から。
  - HTTP: `GET /api/plugins`、`POST /api/plugins/preview|install|enable`、`DELETE /api/plugins?id=` (studio プラグイン、変更系は loopback のみ)。
- **つまり今の「置き場」は LUDIARS org の GitHub リポジトリそのもの**。どのリポのどのフォルダにプラグインがあるかの一覧 (索引) は無く、
  取得元は人が文字で入れる。
- **画面が無い**: studio.md にはタブ「プラグイン」があるが、ネイティブの `StudioView` (段 3 後) にはプラグインのページが無く、API
  エクスプローラからしか操作できない。
- plugin pack = C ABI の共有ライブラリ (`include/ergo/studio/plugin_abi.h`) + 同梱の `*.studio.json`。
- パッケージ・ハブ (`2026-10-14-package-hub-*.md`): C++ パッケージの版固定 (マニフェストの sha)。

## 足すもの (既存の取得の仕組みはそのまま使う)

1. **索引**: 下記の `index.json`。各項目の `source` は今の取得元の書式 (`owner/repo:subpath@rev`) にし、「入れる」は今の
   preview → install をそのまま呼ぶ (取得・検証・ビルド・台帳を作り直さない)。
2. **ランチャーのプラグイン画面**: 索引の一覧 (用途・様式で絞る) と、入れた pack (台帳) の有効/無効/削除。今の API を使う。
3. **ビルド済み配布 (任意・後回し)**: 今はソースを取ってビルドする。ビルド済みを配るのは必要になってから (`artifacts`)。

## 置き場の形

置き場は「索引 (どんなプラグインがあり、どこから、どの版を取るか)」と「実体 (ソースか、ビルド済みのもの)」に分ける。

### 索引

`index.json` (版 1):
```json
{"version":1,"plugins":[{
  "id":"ludiars/particle",              // 一意。<owner>/<name>
  "title":"パーティクル", "summary":"...",
  "domains":["game","cglive"],          // 用途 (キュレーションの絞り込み)
  "styles":["cpp-ludiars"],             // 対応する様式 (空 = 問わない)
  "kind":"builtin|pack",                // builtin = Ergo 内蔵 (有効化だけ)、pack = 取得が要る
  "source":"LUDIARS/Xxx:studio_pack@<40 桁 sha>",   // 今の取得元の書式 (plugin_source.h)。そのまま preview に渡す
  "artifacts":[{"platform":"windows-x64","url":"...","sha256":"..."}],   // ビルド済み (任意)
  "tables":["particle.emitter"],        // 持つデータテーブル (別紙 §3)
  "abi":1                               // plugin_abi の版
}]}
```

- 索引は版付きで、取得側は `sha256` (ビルド済み) か 40 桁 sha (ソース) で中身を確かめる。合わなければ入れない。
- 置き場に載せる・更新するのは Revisor の審査を通ったものだけ (索引のファイル自体を PR で更新する)。

### Studio 側 (ランチャーの「プラグイン」画面)

- 索引を読み、用途・様式で絞って一覧。「入れる」で: 取得 → 検証 (sha / sha256、`abi`) → 台帳に記録 (今の ledger をそのまま使う) → 有効化。
- 取得は loopback の画面だけ (公開名からはできない)。取得先はホストの許可リスト (置き場のホストだけ) にする。
- 通信は Portus の `net` (タイムアウト・リダイレクト禁止・証明書検証)。
- プロジェクトのプロフィール `plugins` に id と版を書けば、インテークで足りないプラグインを入れる (オーケストレーション)。

## 置き場をどこに置くか (決めてほしい点)

実体は今どおり LUDIARS の各リポ (取得の仕組みが GitHub の sparse fetch なので)。決めるのは索引の置き場だけ。

| 案 | 中身 | 良い点 | 気になる点 |
|---|---|---|---|
| C (推奨に変更) Ergo リポの中に索引を置く | `plugins/index.json` (Ergo の Revisor 審査で更新) | 新しいリポ・サービスが要らず、既存の取得の仕組みと同じリポで完結。すぐ始められる | Ergo の版と索引の更新が混ざる (索引だけの PR にすれば実害は小さい) |
| A 新しいリポ 1 つに索引を置く | `index.json` | 索引の更新を Ergo と分けられる | リポが 1 つ増える。名前を決める必要がある |
| B Portus で小さな配信サービスを立てる | 索引 API + ビルド済みの配信 | 非公開のプラグインやビルド済みも配れる | サーバの運用が増える。今の取得の仕組み (GitHub) と二重になる |

リポ / サービスの名前と略称は neco が決める (決まるまでフォルダも登録も作らない)。

## テスト

- 索引の parse (版・必須欠け・不正な id / sha / url)、用途・様式での絞り込み、取得物の検証 (sha256 不一致で入れない、abi 不一致で入れない)、
  台帳への記録と有効化 (偽の取得で)。公開名からの取得の拒否。

## 段階

1. 索引の形と Studio 側の一覧・絞り込み (ローカルの索引ファイルで)。
2. 取得・検証・台帳記録 (置き場の決定後)。
3. プロフィールの `plugins` からの自動導入 (インテーク)。
