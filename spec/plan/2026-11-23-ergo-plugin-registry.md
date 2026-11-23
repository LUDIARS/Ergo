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

## 置き場 (決定 2026-11-23, neco)

「プラグインは増やしたいので Ergo の外で管理し、証明書による信頼情報をおきます」。

- 置き場は **Ergo の外の独立したリポ** (案 A)。索引・発行者の証明書・失効リストを持つ。Ergo には置かない (案 C は不採用)。
  プラグインの実体は今どおり各発行者のリポ (取得は既存の preview → install)。
- リポの名前と略称は neco が決める (決まるまでフォルダも登録も作らない)。以下「置き場リポ」と書く。
- 増やすことが前提なので、取得元の制限を GitHub の owner 許可リスト (`ERGO_PLUGIN_OWNERS`) から **証明書による信頼** に移す。
  許可リストは「証明書の無いものを手で入れる」場合の最後の手段として残す (既定は LUDIARS のまま)。

### 証明書による信頼

```
LUDIARS ルート証明書 (Studio に同梱、秘密鍵は人が保管・オフライン)
  └─ 発行者の証明書 (発行者ごと。ルートが署名。期限・発行者 id・許す id の接頭辞を持つ)
       └─ プラグインの版の署名 (発行者が署名。対象 = 索引の項目の正規化 JSON)
```

| もの | 置き場リポでの場所 | 中身 |
|---|---|---|
| ルート証明書 | `trust/root.pem` (Studio にも同梱。Studio は同梱の方だけを信頼の起点にする) | Ed25519 公開鍵の X.509 |
| 発行者の証明書 | `publishers/<publisher>.pem` | ルートが署名。`subject` = 発行者 id、拡張に許す plugin id の接頭辞 (`ludiars/` など)、有効期限 |
| 索引 | `index.json` | 項目ごとに `publisher` と `signature` (base64) を持つ。署名の対象は項目から `signature` を除いた正規化 JSON (キー順・空白なし) |
| 失効リスト | `trust/revoked.json` + `trust/revoked.sig` (ルートが署名) | 失効した発行者の証明書 (指紋) とプラグインの版 (id + rev) |

Studio の検証 (どれか 1 つでも外れたら入れない・一覧で「信頼できない」と出す):
1. 発行者の証明書が同梱のルートで検証でき、期限内で、失効リストに無い。
2. 項目の署名が発行者の証明書の鍵で検証できる。plugin id が証明書の許す接頭辞に入っている。
3. 項目の `source` の rev が 40 桁 sha で、取得した中身がその sha である (既存の preview の確認)。ビルド済みなら `sha256` が合う。
4. 失効リストに版が載っていない。失効リスト自体もルートの署名を検証する。
5. 台帳には信頼の結果 (発行者 id・証明書の指紋・検証した時刻) も記録する。起動時に失効リストで再検証し、失効したものは読み込まない。

- 暗号は OpenSSL (Portus C++ の `crypto` と同じく自作しない)。Ed25519 + X.509。
- 秘密鍵: ルートは人がオフラインで保管。発行者の鍵は各発行者が持つ。AI は秘密鍵を扱わない
  (署名は人か、鍵を持つ CI が行う。LUDIARS の発行者の鍵の置き場は Vault で人が登録)。
- 置き場リポへの登録・更新は Revisor の審査を通す (索引・証明書・失効リストの PR)。

### 段階 (置き場)

1. 置き場リポの形 (index / publishers / trust) と、Studio の検証の純粋関数 (証明書の連鎖・署名・失効・接頭辞) とテスト。
   テストの鍵はテストの中で作る (固定の鍵の文字列を置かない)。
2. ランチャーのプラグイン画面 (一覧・信頼の表示・入れる/外す)。入れるは既存の preview → install に検証を挟む。
3. ルート証明書の作成と LUDIARS 発行者の証明書の発行 (人の作業)、既存の LUDIARS プラグインの登録。

## テスト

- 索引の parse (版・必須欠け・不正な id / sha / url)、用途・様式での絞り込み、取得物の検証 (sha256 不一致で入れない、abi 不一致で入れない)、
  台帳への記録と有効化 (偽の取得で)。公開名からの取得の拒否。
- 信頼: 正しい連鎖は通る / 別のルートで署名した発行者・期限切れ・失効した発行者・失効した版・署名の改ざん (1 バイト)・
  許す接頭辞の外の id は入れない / 失効リストの署名が合わなければ失効リストごと信頼しない (= 何も入れない側に倒す)。

## 段階

1. 索引の形と Studio 側の一覧・絞り込み (ローカルの索引ファイルで)。
2. 証明書による信頼の検証 (上の「段階 (置き場)」1〜2)。
3. 置き場リポの作成 (名前の決定後) とルート・発行者の証明書 (人の作業)。
4. プロフィールの `plugins` からの自動導入 (インテーク)。
