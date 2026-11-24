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
- 置き場に別の名前は付けない。リポは **`LUDIARS/ErgoPlugin`** (neco 2026-11-24)。以下「ErgoPlugin」と書く。
- プラグインサーバは **GitHub** を前提にする (専用のサーバは立てない)。Studio は GitHub から索引と信頼情報を取る。
- 署名は **GitHub Actions** で行う (neco 2026-11-24)。人や AI が手元で署名しない。
- 増やすことが前提なので、取得元の制限を GitHub の owner 許可リスト (`ERGO_PLUGIN_OWNERS`) から **証明書による信頼** に移す。
  許可リストは「証明書の無いものを手で入れる」場合の最後の手段として残す (既定は LUDIARS のまま)。

### 証明書による信頼

署名を GitHub Actions に寄せるので、発行者ごとに鍵を持たせず、**ErgoPlugin の Actions が持つ署名用の鍵 1 本**で
索引と失効リストに署名する。発行者 (どのプラグインを載せてよいか) は鍵ではなく、審査済みの `publishers/*.json` で決める。

```
LUDIARS ルート証明書 (Studio に同梱。秘密鍵は人がオフラインで保管し、署名用証明書の発行と更新にだけ使う)
  └─ 署名用証明書 (ルートが署名。有効期限つき。秘密鍵は ErgoPlugin の Actions の secret)
       ├─ 索引の署名 (対象 = index.json の正規化 JSON 全体)
       └─ 失効リストの署名
```

| もの | ErgoPlugin での場所 | 中身 |
|---|---|---|
| ルート証明書 | `trust/root.pem` (Studio にも同梱。Studio は同梱の方だけを信頼の起点にする) | Ed25519 公開鍵の X.509 |
| 署名用証明書 | `trust/signing.pem` | ルートが署名した Ed25519 の X.509。有効期限は短め (1 年)。更新は人がルート鍵で行い PR で差し替える |
| 発行者 | `publishers/<publisher>.json` | 発行者 id、許す plugin id の接頭辞 (`ludiars/` など)、許す GitHub owner (取得元の `owner`) |
| 索引 (署名前) | `index.json` | 上の項目に `publisher` を足したもの。署名は持たない (審査する人と AI が読む正本) |
| 失効リスト (署名前) | `trust/revoked.json` | 失効した署名用証明書 (指紋) とプラグインの版 (id + rev) |
| 配布物 | GitHub Release `index-<連番>` の添付 | `index.signed.json` (索引 + `signature` + `signed_at` + `serial`)、`revoked.signed.json`、`signing.pem` |

#### Actions の流れ

1. ErgoPlugin の main に push されたとき (= Revisor の審査を通ってマージされ、GitHub に公開されたとき) に動く。
2. 検査: 索引の形、各項目の `publisher` が `publishers/` にあり、plugin id がその接頭辞に入り、`source` の owner が
   その発行者の owner に入り、rev が 40 桁 sha であること。外れたら署名しない (Actions は失敗で止まる)。
3. 正規化 JSON (キー順・空白なし・UTF-8) を作り、secret の鍵で Ed25519 署名する (OpenSSL)。
4. `serial` (単調増加、前の Release + 1) を入れ、Release `index-<serial>` に添付する。**main には書き戻さない**
   (main は Revisor が公開する系列なので、bot のコミットを入れない)。
5. 鍵は Actions の secret (`ERGO_PLUGIN_SIGNING_KEY`) にだけ置く。登録・更新は人が GitHub の画面で行う。
   署名の job は main の push だけで動かし、PR やフォークからは動かさない (secret を渡さない)。

#### Studio の取得と検証

取得先は `https://github.com/LUDIARS/ErgoPlugin/releases/latest/download/` の固定の 3 ファイルだけ
(ホストの許可リストは github.com と、Release の実体が置かれる objects.githubusercontent.com)。
通信は Portus の `net` (タイムアウト・証明書検証。Release のダウンロードはリダイレクトするので、許可リストのホストへのリダイレクトだけ追う)。

どれか 1 つでも外れたら入れない・一覧で「信頼できない」と出す:
1. 署名用証明書が同梱のルートで検証でき、期限内で、失効リストに無い。
2. 索引の署名が署名用証明書の鍵で検証できる。`serial` が前に受け取った値より小さくない (古い索引への巻き戻しを断る)。
3. 失効リストの署名が検証できる。合わなければ失効リストごと信頼しない (= 何も入れない側に倒す)。版が失効リストに無い。
4. 項目の `source` の rev が 40 桁 sha で、取得した中身がその sha である (既存の preview の確認)。ビルド済みなら `sha256` が合う。
5. 台帳には信頼の結果 (発行者 id・索引の serial・署名用証明書の指紋・検証した時刻) も記録する。起動時に失効リストで再検証し、
   失効したものは読み込まない。

- 暗号は OpenSSL (Portus C++ の `crypto` と同じく自作しない)。Ed25519 + X.509。
- 秘密鍵: ルートは人がオフラインで保管。署名用の鍵は人が作って Actions の secret に登録する。AI は秘密鍵を扱わない
  (作らない・読まない・登録しない)。
- ErgoPlugin への登録・更新は Revisor の審査を通す (索引・発行者・証明書・失効リストの PR)。署名はその後の Actions だけ。

### 段階 (置き場)

1. ErgoPlugin の形 (index / publishers / trust) と署名の Actions、その検査と署名の手順のテスト
   (テストの鍵はテストの中で作る。固定の鍵の文字列を置かない)。
2. Studio の検証の純粋関数 (証明書の連鎖・署名・serial・失効・接頭辞) とテスト。
3. ランチャーのプラグイン画面 (一覧・信頼の表示・入れる/外す)。入れるは既存の preview → install に検証を挟む。
4. ルート証明書と署名用の鍵の作成、secret の登録 (人の作業)。既存の LUDIARS プラグインの登録。

## テスト

- 索引の parse (版・必須欠け・不正な id / sha / url)、用途・様式での絞り込み、取得物の検証 (sha256 不一致で入れない、abi 不一致で入れない)、
  台帳への記録と有効化 (偽の取得で)。公開名からの取得の拒否。
- 信頼: 正しい連鎖は通る / 別のルートで署名した署名用証明書・期限切れ・失効した証明書・失効した版・索引の改ざん (1 バイト)・
  serial の巻き戻しは入れない / 失効リストの署名が合わなければ失効リストごと信頼しない (= 何も入れない側に倒す)。
- Actions の検査: 発行者の接頭辞の外の id・発行者の owner の外の取得元・40 桁でない rev は署名しない。

## 段階

1. 索引の形と Studio 側の一覧・絞り込み (ローカルの索引ファイルで)。
2. 証明書による信頼の検証 (上の「段階 (置き場)」1〜2)。
3. ErgoPlugin の作成と、ルート証明書・署名用の鍵の登録 (人の作業)。
4. プロフィールの `plugins` からの自動導入 (インテーク)。
