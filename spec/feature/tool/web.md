# ブラウザで開く Ergo Studio (`web` / `assets` プラグイン) {#SPEC-ergo-web}

依頼 (2026-11-23, neco): 「ErgoStudio を限定 Web 公開して、アセットの一覧、ライティング調整が出来るようにする。
またゲームのビルドと実行と実行しながらの調整を出来るようにする。Amicii をサンプルとしてセットアップ Web 公開まで」。
遠隔プレイは「Odeum や Custos に WebRTC でのリモートプレイ実装あるのでそれ使って」(同日)。

ネイティブ版の設計 (spec/plan/2026-11-05-ergo-studio-native.md) はブラウザ版を廃止した (J9)。
この仕様は、その判断のうち「ブラウザ UI を TS で二重に保守しない」を守ったまま、**ネイティブの Studio が
1 枚のページを配る**形で限定公開を足す。ページは他のプラグインの既存 API だけを使い、新しい変更系の API は持たない。

## 構成

| 部分 | 場所 | 役割 |
|---|---|---|
| `web` プラグイン | `tools/studio/plugins/web/` | ページ (`page/index.html`・`app.js`・`style.css`、実行ファイルに埋め込み) と `GET /web/api/config` |
| `assets` プラグイン | `tools/studio/plugins/assets/` | プロフィールの `assets` フォルダの一覧と、画像・文字ファイルの表示 (読み取りのみ) |
| 公開名の検査 | `src/studio/net/request_guard.cpp` | `ERGO_STUDIO_PUBLIC_HOSTS` の名前を、Access を通った要求にだけ許す |
| 遠隔プレイ | Odeum `odeum-runner` (Custos の C++ 版) | ビルド・実行・停止・WebRTC の画面配信と操作。ページはそのページを枠 (iframe) で出す |

## ページのタブ

| タブ | 内容 | 使う API |
|---|---|---|
| ゲーム | 遠隔プレイの枠 (`ERGO_STUDIO_PLAY_URL`) と、プロフィールのサービスの起動・再起動・停止・ログ | iframe、`/services/api/services`・`/api/services/<code>/{control,logs}` |
| ライティング | `look/...` の変数をグループ別にスライダー・数値欄・切り替えで出す。プリセットの読み込み (`look.apply`) と保存 (`look.preset`) | WS `/variable/ws` (role `ui`)、`/lookdev/api/presets` |
| 調整値 | ゲームが出す全変数。名前で絞り込み。版の保存・差分・戻す | WS `/variable/ws`、`/tuning/api/versions` |
| アセット | プロフィールの `assets` フォルダを順にたどる一覧 (種類・大きさ・更新時刻)、画像と文字ファイルの表示 | `/assets/api/{roots,list,file}` |

変数の扱いは lookdev タブ・tuning タブ (ネイティブ) と同じ取り決め (lookdev.md・game-ops.md)。
`look/Capture` と `look.apply` / `look.preset` / `look.capture_*` は調整の一覧に出さない。
撮影して投稿 (lookdev の `capture-dir` / `post`) はページから行わない (画面は遠隔プレイで見える)。

## 不変条件

### WEB-INV-01 listen は loopback のまま {#SPEC-ergo-web-loopback}

公開しても `ERGO_HOST` は `127.0.0.1` のまま。外からはトンネル (cloudflared) だけが届く。
`ERGO_STUDIO_PUBLIC_HOSTS` の名前は、`Cf-Access-Jwt-Assertion` ヘッダー付きの要求だけ通す
(`net::cross_site_problem(GuardPolicy, ...)`)。Origin を付けるなら同じ名前の https であること。
一覧に無い名前・ポート付きの名前は従来どおり拒む。署名の検証は Access 側に任せる (ヘッダーの有無で
「Access を通ってきた」ことだけを見る。loopback に届く経路はトンネルとこの PC だけ)。

### WEB-INV-02 ページは新しい変更系を持たない {#SPEC-ergo-web-no-new-mutation}

ページが使う変更は、変数の `set` (WS)、プリセットと版の保存、サービスの起動・停止 (Concordia への一報付き)
だけで、どれも既存のプラグインの API。次はページから (公開名から) できない:

- `studio` (AI への指示・プラグインの導入) と `lesson`: もとから loopback の Host だけ (ST-INV-03)
- `git` の変更系 (commit / switch / pull / push / fetch): この PC からだけ (`check_mutation` を足した)
- アセットの書き込み・移動・削除: `assets` は GET だけ

### WEB-INV-03 アセットの表示 {#SPEC-ergo-web-assets}

フォルダはプロフィールの `assets` (`[{"name", "path"}]`、`root` からの相対) だけ。
要求のパスは `resolve_within()` でそのフォルダの中に限る。`.` で始まる名前 (`.git` など) は出さない。
1 フォルダ 2000 件まで (超えたら `truncated`)。中身を返すのは PNG / JPEG / GIF / WebP / BMP と、
data・shader 種別の文字ファイル (`text/plain`) だけ、16 MB まで。SVG は返さない (スクリプトを含められる)。
応答には `content-security-policy: default-src 'none'; sandbox` と `nosniff` を付ける。

### WEB-INV-04 ページの CSP {#SPEC-ergo-web-csp}

`default-src 'self'`、`connect-src 'self'` (同じ名前の WS を含む)、`frame-src` は `ERGO_STUDIO_PLAY_URL` の
origin だけ (未設定なら `'none'`)、`frame-ancestors 'none'`。サーバからの値は `textContent` で出し、HTML として入れない。

## 遠隔プレイ (Odeum runner)

ビルド・実行・停止・画面配信・操作は Odeum の `odeum-runner` (Custos の C++ 移植) が持つ。Studio は作らない。

- アプリの定義 (ビルド・実行のコマンド、窓の名前、操作キー) はゲームのリポが持つ `apps.json`
  (`CUSTOS_APPS_FILE`)。Amicii は `tools/runner/amicii.apps.json`。
- 画面は ffmpeg の gdigrab で窓を撮って H.264 にし、WebRTC で送る。遠隔の NAT 越しは TURN
  (`CUSTOS_TURN_KEY_ID` / `CUSTOS_TURN_KEY_API_TOKEN`、Cloudflare) が要る。
- runner も loopback で listen し、公開名は `LUDIARS_ALLOWED_HOSTS` / `CUSTOS_ALLOWED_ORIGINS` で許す。
- ページは runner のページを枠で出す。runner とページは同じ登録ドメインの別の名前にし、
  同じ Access アプリで守る (同一サイトなので Access のクッキーが枠の中でも届く)。

## 公開 (Excubitor + Cloudflare Access)

| 項目 | 値 |
|---|---|
| Studio | `ergo-studio` (Excubitor)。env `ERGO_STUDIO_PUBLIC_HOSTS=<Studio の公開名>`、`ERGO_STUDIO_PLAY_URL=https://<runner の公開名>/`、`ERGO_PLUGIN_DIR=<ゲームの pack フォルダ>` |
| runner | `odeum-runner` (Excubitor)。env `CUSTOS_APPS_FILE`、`LUDIARS_ALLOWED_HOSTS=<runner の公開名>`、`CUSTOS_ALLOWED_ORIGINS=https://<runner の公開名>` |
| Access | 2 つの名前を 1 つの Access アプリ (許可は人間が決めた相手だけ) に入れ、ローカルのトンネルの経路を張る (`cf-access-publish` スキル) |

公開名・許可する相手・TURN の鍵は、拠点の設定 (Excubitor) に置き、このリポには書かない。

## テスト

`tests/studio/test_studio_web.cpp`: 公開名の解釈、Access ヘッダーの有無と Origin、従来の名前の扱い、
プロフィールの `assets`、種類と表示の可否、一覧 (フォルダ優先・隠しファイル・外へ出るパス)、
`assets` プラグインの一覧・表示・415 / 404 / 405、ページの CSP と config。

実施していないこと (2026-11-23 時点の本書作成時): ブラウザでの表示確認、Access 越しの WebSocket。
