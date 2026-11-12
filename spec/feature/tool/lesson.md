# 授業連携 (`lesson` プラグイン) {#SPEC-ergo-lesson}

依頼 (2026-11-11): odeum-lesson (LUDIARS/Odeum、AI が進める授業) と Ergo Studio をつなぐ。生徒は
ブラウザで授業に参加し、手元では Ergo Studio と、`ergo_bind` (`/variable/ws`) で Studio の `variable`
プラグインにつないだゲームを動かしている。Studio は次の 2 つを自動で行う。

- **生徒**: 動かしているゲームが公開する変数が課題の手順の条件 (`check`) を満たしたら、その手順の達成を
  odeum-lesson に報告する。
- **実演** (授業で実演を映している教員の PC): 授業の進行が実演の合図 (`demo`) に達したら、ゲームの機能の
  ON / OFF (bool 変数) を切り替える。

## 置き場所

ネイティブ版 Ergo Studio (`tools/studio`) の組み込みプラグイン。`IPluginHealth` と `IPluginView` に加えて、
Studio 自身の (loopback の) サーバに `/lesson/api/*` の 3 つの経路を持つ (窓の無い構成とスクリプト用、
[LS-INV-06](#SPEC-ergo-lesson-routes))。WS の経路は持たない。

| 層 | ファイル | 責務 |
|---|---|---|
| 接続文字列 (純関数) | `plugins/lesson/lesson_link` | 接続文字列の検査と分解 |
| 取り決め (純関数) | `plugins/lesson/lesson_protocol` | pair / state / progress の JSON ⇄ 構造体、失敗の分類 |
| 通信 | `plugins/lesson/lesson_client` | `platform::IHttpClient` での 3 つの呼び出し |
| 判定 (純関数) | `plugins/lesson/step_check` | 手順の条件と変数の値の比較 |
| 実演 | `plugins/lesson/demo_apply` | ゲームが公開している値と授業の状態の突き合わせ、切り替えの依頼 |
| 状態機械 | `plugins/lesson/lesson_session` | 「1 回ポーリングする」で進む。スレッドも時計も持たない |
| プラグイン | `plugins/lesson/lesson_plugin` | ワーカースレッド、表示用の状態、health |
| 経路 | `plugins/lesson/lesson_routes` | `/lesson/api/state` / `connect` / `disconnect` (変更系の防御付き) |
| 表示用の状態の JSON | `plugins/lesson/lesson_shown_json` | タブと同じ内容を JSON に (token・コードは含まない) |
| 変更系の防御 | `plugins/common/mutation_guard` | ST-INV-03 の検査 (`studio` プラグインと共用。`plugins/studio/` から移した) |
| ゲームの変数 | `plugins/lesson/game_variables.h` | lesson が必要とする変数アクセスの interface (`IGameVariables`) |
| 橋渡し | `tools/studio/lesson_variables` | `IGameVariables` を `variable` プラグインで実装する (組み立て側) |
| 画面 | `tools/studio/views/lesson_view` | タブ「授業」 |
| クリップボード | `include/ergo/studio/platform/clipboard.h`、`src/studio/platform/win_clipboard.cpp` | OS のクリップボードの文字列を読む |

`plugins/lesson/` は `variable` プラグインのヘッダを include しない。`builtin_plugins()` が
`GameVariableBridge` を 1 つ作り、`variable` と `lesson` の両方の factory に渡す。プラグインは読み込み順に
破棄されるので、`variable` は橋から外れてから破棄される (`make_bridged_variable_plugin`)。外れた後、
橋はどの変数も「公開されていない」と答える。

## 通信の取り決め {#SPEC-ergo-lesson-wire}

odeum-lesson 側と同じ内容を持つ。片方だけを変えない。WebSocket ではなく HTTP(S) のポーリングにしたのは、
Ergo の WS クライアントに TLS が無いため。

### 接続文字列 {#SPEC-ergo-lesson-link}

`<origin>/v1/tool#<CODE>` (例: `https://lesson.example.org/v1/tool#7K2M9QX4TB`、
`http://127.0.0.1:4410/v1/tool#7K2M9QX4TB`)。生徒が授業ページでコピーする。

- `origin` は `http://` か `https://` + ホスト名 [`:ポート`]。ユーザ情報 (`@`) とクエリ (`?`) は不可。
- パスは `/v1/tool` ちょうど。
- `CODE` は Crockford base32 の 10 文字 (`0-9A-HJKMNP-TV-Z`)。小文字は大文字にそろえる。それ以外は拒否する。
- 前後の空白と改行は無視する。合わないものは、理由を画面に出して拒否する (理由に入力の文字列は含めない)。

### pair

`POST <origin>/v1/tool/pair`、`Content-Type: application/json`、本文 `{"link":"<CODE>","tool":"ergo-studio"}`

- `200 {"token":"<小文字 16 進 32 桁>","role":"student"|"presenter","class":{"title":"..."},"poll_ms":2000}`
- 失敗は 4xx の `{"error":"<code>"}`: `link_not_found` (404)、`rate_limited` (429)、`invalid_request` (400)

### state

`GET <origin>/v1/tool/state`、`Authorization: Bearer <token>`。`poll_ms` (1000〜10000 に丸める) ごとに呼ぶ。

```json
{
  "role": "student",
  "class": {"title": "アクションゲームの作り方"},
  "features": {"hit_stop": "feel.hit_stop", "knockback": "feel.knockback", "camera_shake": "feel.camera_shake"},
  "assignment": null,
  "demo": {"seq": 0, "states": {}}
}
```

- `features`: 機能名 → ゲームの **bool** 変数の名前 (空でもよい)。
- `assignment`: `null` か
  `{"id","title","active","steps":[{"id","label","done","check":{"variable","op","value"}}]}`。
  `check` は手順ごとに省略できる。`op` は `gt ge lt le eq ne`、`value` は数値・bool・文字列。
- `demo`: 実演のときだけ付く。`seq` は実演の合図に達するたびに増え、`states` は進行がこれまでに設定した
  機能 → bool。生徒には付かず、実演では `assignment` が `null`。
- `401 {"error":"tool_unlinked"}`: token が無効になった (授業の終了、生徒の削除、別の場所での再接続)。

### progress (生徒だけ)

`POST <origin>/v1/tool/progress`、`Authorization: Bearer <token>`、`Content-Type: application/json`、
本文 `{"assignment_id":"restore-feel","step":2,"done":true}` (`step` は `assignment.steps` の 0 始まりの番号)

- `200 {"ok":true}` / `409 {"error":"not_assigned"}` (課題が終わった) / `401 tool_unlinked`

## 動作

### 生徒 {#SPEC-ergo-lesson-student}

ポーリングのたびに、`check` があり、サーバ上で `done` でなく、このセッションがまだ報告していない手順に
ついて、変数の今の値で条件を調べる。満たしていたら `done:true` を 1 回だけ送る。

- 比較: `gt ge lt le` は両辺が数値 (変数の kind が `int32` / `int64` / `float` / `double`)。`eq ne` は
  数値どうし・bool どうし・文字列どうし。変数が無い、または型が合わないときは「満たしていない」(エラーに
  しない。`ne` でも同じ)。
- 送信が失敗したら報告済みにせず、次のポーリングで送り直す。一度満たした手順は、送り直すまでに値が
  戻っていても報告する。
- `409 not_assigned` を受けたら、その課題への報告をやめる (エラー表示にしない)。`active` が `false` の
  課題にも報告しない。
- `check` の無い手順には触れない (生徒が授業ページで印を付ける)。

### 実演 {#SPEC-ergo-lesson-presenter}

`demo.states` は進行がこれまでに設定した機能 → bool の**全体**なので、Studio はポーリングのたびに
`demo.states` の各機能について、`features` にある変数の**ゲームが今公開している値**と欲しい値を比べ、
違えば bool の設定を `variable` プラグイン経由で頼む (`seq` が同じでも比べる)。これで、実演中にゲームを
起動し直した (変数が既定値で bind し直された)、別の道具が値を変えた、前の依頼が届かなかった、のどれでも
次のポーリングで授業の状態に戻る。

| 状態 | 意味 | 画面 |
|---|---|---|
| 反映済み | ゲームが公開している値が欲しい値と同じ | `ON` / `OFF` |
| 依頼済み | 値が違ったので、このポーリングで設定を頼んだ | 「切り替えを頼みました (ゲームが従うのを待っています)」 |
| 従っていない | 前のポーリングでも頼んだのに、まだ値が違う。今回も頼んだ | 「ゲームが従っていません (…毎回頼み直しています)」(赤) |
| ゲーム待ち | 変数が公開されていない (ゲームが未接続、名前が無い) | 「未反映 (ゲームがこの変数を公開するのを待っています)」 |
| bool でない | 変数が bool でない。設定しない | 「未反映 (ゲームの変数が bool ではありません)」 |
| 変数なし | `features` に無い機能 (黙って捨てない) | 「対応する変数がありません」 |

- 「反映済み」はゲームが公開した値で判断する。設定の依頼を送ったことではない。
- 依頼は 1 回のポーリングで機能ごとに高々 1 回。従わないゲーム (`read_only` の変数、ゲームが自分で値を
  戻す) にも、ポーリング間隔 (1〜10 秒) ごとに小さな `set` が 1 つ届くだけで、溢れさせない。
- 欲しい値が変わらない限り「従っていない」は新しい `seq` でも消えない。値が一度そろえば「反映済み」に戻り、
  その後にまたずれたら「依頼済み」から数え直す。
- 状態の変化 (と「従っていない」になったこと) はログに 1 行ずつ出す (機能名・変数名・ON/OFF・状態だけ)。
- ゲーム側の前提: `ergo_bind` は Studio からの `set` を適用したあと、その変数の今の値を `value` で
  送り返す ([`bind.md`](../module/bind.md))。これが無いと Studio の手元の値が古いままになり、「反映済み」を
  判断できない。

### 接続が切れたとき

どの呼び出しでも `401 tool_unlinked` を受けたら、token を捨ててポーリングをやめ、
「接続が切れました。授業ページで接続文字列を取り直してください」と出す。それ以外の失敗 (つながらない、
時間切れ、HTTP の状態 + サーバの `error`、取り決めと違う応答) は種類を分けて画面に出し、接続は保ったまま
次のポーリングで試し直す。

## 画面 (タブ「授業」) {#SPEC-ergo-lesson-view}

- 「貼り付けてつなぐ」/「切る」。**ネイティブ版 Studio の UI には文字入力欄がまだ無い**ので、接続文字列は
  クリップボードから読む (授業ページでコピー → ボタンを押す)。読んだ文字列は画面に出さない。
- 状態: 未接続 / 接続中 / 接続済み (生徒 | 実演)、接続先のホスト名、授業名、切れた理由、直近の失敗。
- 生徒: 課題名と手順の一覧 (`✓` = 済み、`[自動]` = `check` がある)。
- 実演: 機能ごとの ON / OFF / 未反映。

health (`/api/health` の `lesson`) は `{ok:true, linked, role:"none"|"student"|"presenter"}` だけ。

## Studio のサーバの経路 {#SPEC-ergo-lesson-http}

窓の無い Studio (`ergo_studio --headless`) にはタブも貼り付けも無いので、同じ操作を Studio 自身のサーバに置く。
タブと同じことしかできない (接続文字列の受け渡し、切る、タブに出ている内容を読む)。

| 経路 | 本文 | 応答 |
|---|---|---|
| `GET /lesson/api/state` | — | `200` タブと同じ内容: `{connecting, link_error, linked, role, host, class, end, failure, assignment, demo}` (`plugins/lesson/lesson_shown_json.h` に形)。token・接続コード・貼り付けた文字列・ポートは含まない |
| `POST /lesson/api/connect` | `{"link":"<接続文字列>"}` | `202 {ok, connecting:true}` (つなぐのはワーカー。結果は state で見る) / `400 invalid_request` (JSON でない、`link` が文字列でない) / `400 invalid_link` (タブと同じ拒否理由。入力は含めない) / `409 already_linked` (接続中・接続済み。タブのボタンと同じく先に切る) |
| `POST /lesson/api/disconnect` | `{}` | `200 {ok:true}` (= 切る) |

- 失敗は `{error, message}`。方法違いは `405`、他の経路は `404`。
- 選んだ理由: 環境変数で接続文字列を渡す方法は、接続コードがプロセスの環境 (他の利用者や子プロセスから見える) に
  残り、LS-INV-05 (環境変数を持たない) とも合わない。経路なら Studio が既に持つ防御 (下の LS-INV-06) をそのまま使え、
  接続を切る・状態を読むこともできる。

## 不変条件

### LS-INV-01 秘密はメモリだけ {#SPEC-ergo-lesson-secrets}

token と接続コードはメモリにだけ持つ。ディスクに書かず、ログに出さず、画面にも health にも出さない。
画面に出すのは接続先のホスト名だけ (ポートも出さない)。サーバの `error` は英小文字・数字・`_` の
40 文字以内のときだけ表示する。

### LS-INV-02 進み具合は戻さない {#SPEC-ergo-lesson-latched}

`done:false` は送らない。一度報告した手順の印を Studio から外すこともしない。

### LS-INV-03 代わりの経路を持たない {#SPEC-ergo-lesson-no-fallback}

通信は `platform::IHttpClient` (`make_system_http_client()`) だけ。実装の無い環境では「未対応」の失敗をそのまま
画面に出し、別の経路に切り替えない。クリップボードも同じ。取り決めと違う応答は既定値で埋めずに失敗にする。

| 環境 | `make_system_http_client()` |
|---|---|
| Windows | WinHTTP (OS の TLS・proxy・証明書ストア) |
| Windows 以外、`-DERGO_WITH_CURL=ON` | managed libcurl (`src/studio/platform/curl_http_client.cpp`)。TLS は libcurl の backend (Linux は OpenSSL) と configure 時の CA bundle。証明書とホスト名の検証は常に有効。proxy は環境変数 (`https_proxy` / `no_proxy` …) |
| Windows 以外、curl なし (既定) | 毎回「この build には HTTP が無い (`-DERGO_WITH_CURL=ON` で構成する)」の失敗 |

libcurl を選んだ理由 (2026-11-11): リポに既に managed 依存として pin 済み (`curl-8_19_0`) で、Linux では OpenSSL で
`https://` まで扱え、この環境でも取得・ビルドできた。自前のソケット実装 (http だけ) より `https://` の授業サーバに
そのままつながる。Studio は curl を強制 ON にはしない (素のビルドをネットワーク非依存に保つ規約)。

libcurl 版の約束 (`src/studio/platform/http_request_check` が純関数で検査する):

- `http://` と `https://` だけ (libcurl の他のプロトコルは使わない)。リダイレクトは最大 10 回、`https://` から
  `http://` へは下りない (WinHTTP の既定と同じ)。`https://` を平文に落とすことはない。
- ヘッダの名前・値に改行や NUL があれば送らずに失敗にする。libcurl が勝手に足す `Content-Type`
  (フォーム) と `Expect: 100-continue` は付けない。
- 応答本文は 64 MiB まで。超えたら失敗 (メモリを埋めない)。`timeout_ms` は全体の上限。
- 失敗の文言は libcurl の一般的な文 (`TLS failed: ...` を含む) だけで、URL・ホスト・ポート・ヘッダを含めない。

### LS-INV-04 通信は UI スレッドの外 {#SPEC-ergo-lesson-thread}

HTTP はワーカースレッド 1 本だけが行う。UI スレッドは指示を積み、最後に公開された表示用の状態を読むだけ。
プラグインの破棄でワーカーを止めて join する (呼び出し中なら、その上限時間 5 秒まで待つ)。

### LS-INV-05 接続先 {#SPEC-ergo-lesson-origin}

接続先は貼り付けた (または `/lesson/api/connect` に渡した) 接続文字列の origin だけ。設定・環境変数・ポート番号を
コードに持たない。

### LS-INV-06 経路の防御 {#SPEC-ergo-lesson-routes}

`/lesson/api/*` は Studio のサーバ全体の検査 (`net/request_guard`: 信頼できる Host、別サイトの Origin を拒否) を
通ったうえで、GET 以外は `studio` プラグインと同じ変更系の防御 (`plugins/common/mutation_guard`、ST-INV-03:
loopback の Host、Origin を付けるなら同じ host:port、本文が JSON) を通る。別サイトのページから localhost へ POST して
授業につながせる・切らせることはできない。接続文字列はログに出さない (ログは「経路から接続を始めた」の 1 行だけ)。

## 検証

- `tests/studio/test_studio_lesson.cpp`: 接続文字列の受理 / 拒否の表、取り決めの読み取り (壊れた JSON は
  明示の失敗)、条件の表 (全演算子・型の不一致・変数なし)、生徒の流れ (1 回だけの POST と本文・Bearer、
  サーバで済みの手順は送らない、送信失敗の送り直し、409 で停止、401 で切断)、実演の流れ (毎回の比較、
  依頼済み → 反映済み、ゲームの再起動と別の変更からの復帰、従わないゲームへの 1 回 / ポーリングの依頼と
  「従っていない」、未公開の変数は保留して公開後に反映、対応の無い機能の報告)、`variable` プラグインとの橋渡し、プラグインの
  health とログに秘密が出ないこと。
- `tests/studio/test_studio_lesson_game.cpp`: 本物の `ergo_bind` エンジンを Studio のサーバ (`net::Server` +
  `variable`) に loopback でつなぎ、実演の切り替えが依頼済み → 反映済みになること、エンジンの再接続 (再起動)
  後に戻ること、`read_only` の変数が「従っていない」になることを確かめる。
- `tests/studio/test_studio_platform_http.cpp` (Windows 以外): 要求の検査の表 (scheme、URL、method、ヘッダの
  改行、空の値、timeout、本文の上限) と、libcurl がある build では loopback の実通信 (Studio 自身の
  `net::Server` への GET / POST / PUT / DELETE と本文・ヘッダ、404、リダイレクト、時間切れ、本文の上限、
  `ftp://` の拒否、`https://` を平文のサーバへ向けると TLS の失敗になり平文の要求は届かないこと、試験内の
  小さな listener での chunked / close 区切り / 壊れた chunked)。
- `tests/studio/test_studio_settings.cpp`: `ergo_studio` のコマンドライン (`--help` / `-h`、`--headless`、`PORT` /
  `ERGO_HOST` との組み合わせ、知らないオプション・位置引数・壊れた `PORT` のエラー、知らないオプションが `--help`
  より優先されること) と、`--help` の文面が `read_settings` の読む全オプション・全環境変数を挙げていること。
- `tests/studio/test_studio_lesson_view.cpp`: タブの文言 (未接続、貼り付けの拒否理由、生徒、実演、従わないゲーム、失敗の種類)
  と、画面に token・コード・貼り付けた文字列が出ないこと。
- `tests/studio/test_studio_lesson.cpp` の `StudioLessonRoutes`: 経路の state / connect / disconnect、405 / 404、
  防御 (別ホスト・JSON でない本文・別サイトの Origin は 403 で pair しない)、拒否理由に入力が出ないこと、409、
  state とログに token・コード・ポートが出ないこと、実演の状態名。
- `examples/lesson_action_feel/tests/` と `ergo_example_lesson_action_feel --check`: サンプルのゲームの規則と、観測値が
  切り替えに従うこと (正本は [lesson-action-game.md](lesson-action-game.md) の「検証」)。
- 実際の odeum-lesson とのつなぎ (2026-11-11、この Linux、libcurl 版、`ergo_studio --headless` + サンプル +
  `odeum-lesson`、`/lesson/api/connect` で接続文字列を渡す):
  - 生徒: `--plain` で起動したサンプルに `/variable/ws` から `feel.hit_stop = true` を送ると、サンプルの次の当たりで
    `feel.hit_stop_last_seconds` が 0.08 になり、授業のサーバが生徒のページに `add-hitstop` 済みの `progress` を送った。
    `feel.knockback` でも同じく `add-knockback`。生徒の退出 (`student.eject`) で `tool_unlinked` → 切断。
  - 実演: 実演用の接続文字列でつなぎ、`lesson.start` → `lesson.seek` で「すべて ON」の合図を越えると 3 つが ON に
    なり、実演中にサンプルを `--plain` で起動し直すと次のポーリングで 3 つとも ON に戻った。「すべて OFF」「ヒット
    ストップだけ ON」の合図、別の道具が値を変えたときの戻しも確認。
- 確認していないこと (2026-11-11 時点):
  - `https://` の授業サーバとの接続 (libcurl 版の TLS は loopback の自己署名サーバでの検証失敗と、proxy 越しの
    公開ホストへの到達だけを確かめた)。
  - Windows でのビルドと動作。WinHTTP 経由の通信と `win_clipboard.cpp` は Linux ではコンパイルもしていない。
  - macOS での libcurl 版 (curl 8.19 は SecureTransport を持たないので、OpenSSL などの TLS backend が要る)。
  - 窓のある Studio での表示 (Pictor + Vulkan + GLFW が要る)。`✓` などの字がフォントにあるかも未確認。
  - 上の 2 つの記録 (2026-11-11) は、サンプルが遊べるゲームになる前の、0.5 秒ごとに叩き続けるシミュレーションで取った。
    遊べるゲームにしてからの授業との通しは [lesson-action-game.md](lesson-action-game.md) の「検証」。

## サンプル (`examples/lesson_action_feel`) {#SPEC-ergo-lesson-sample}

Odeum のサンプル講座 (`action-game`) が名前を出す変数を、実際の Ergo モジュールで公開する**遊べる小さなアクションゲーム**
(壁に囲まれたアリーナで敵の波を倒す)。規則・操作・モード・変数の正本は [lesson-action-game.md](lesson-action-game.md)。
授業との連携に関わる所だけここに書く。

| 変数 | 型 | 内容 |
|---|---|---|
| `feel.hit_stop` / `feel.knockback` / `feel.camera_shake` | bool (Studio から書ける) | 演出の ON / OFF。既定は ON、`--plain` で 3 つとも OFF (「演出を抜いた状態」) から始める |
| `feel.super_armor` | bool (Studio から書ける) | 敵のスーパーアーマー (`combat.enemy.super_armor` の別名: true = off 以外。true を書くと off のときだけ attack にする) |
| `combat.<side>.flinch` / `combat.<side>.motion_cancel` | bool (Studio から書ける) | 陣営 (`player` / `enemy`) ごとのひるみ・モーションキャンセル |
| `feel.hit_stop_last_seconds` | double (読み取り専用) | 最後に `ergo_world_time` が実際に止めた秒数 (威力 ÷ 24。剣で 0.083)。一度も止めていなければ 0 |
| `feel.knockback_last_impulse` | double (読み取り専用) | 最後に実際に与えた撃力 (威力 × 3。剣で 6)。一度も与えていなければ 0 |
| `combat.<side>.armor_absorbed` ほか | int64 (読み取り専用) | スーパーアーマーで吸収した当たりの数など (全部は lesson-action-game.md の「何が起きたか」) |

- 観測値はゲームが実際にしたことから作る。`feel.hit_stop` が OFF の間は 0 のまま、Studio から ON にすると次の当たりで > 0 に
  なる (課題の `check` はこれを見る)。OFF に戻しても「最後の値」は残り、遊び直しでも戻さない。
- 授業の実演は bool だけを切り替える (`DemoApplier`)。スーパーアーマーの 3 段階 (`off` / `attack` / `always`) を bool の実演で
  切り替えられるよう `feel.super_armor` を置いた。
- 起動: 既定 (`--auto`) は窓なしで台本の操作が遊び、Studio につないで数秒ごとに状態を出す。`--play` は端末で遊ぶ
  (`--autopilot` で台本の操作が遊ぶ画面を出す。授業の実演用)。どちらも同じ変数を出す。`--check` は Studio なしの自己確認 (ctest)。

### 端末から使う (窓なし) {#SPEC-ergo-lesson-terminal}

ビルドの出力 (`cmake -S . -B <build> -G Ninja` で確認、2026-11-11、Linux):

| 物 | 場所 |
|---|---|
| Studio | `<build>/studio/ergo_studio` (Windows は `ergo_studio.exe`。`studio/` は CMake が全構成で固定している) |
| サンプルのゲーム | `<build>/examples/ergo_example_lesson_action_feel` (Visual Studio などの複数構成の generator では構成名のフォルダが挟まる見込み。未確認) |

Pictor / Vulkan / GLFW の無い build の Studio は常に窓なし (configure で「building the headless server only」と出る)。
窓のある build では `--headless` で窓を出さない。止めるのはどちらも Ctrl+C (SIGINT)。

`ergo_studio --help` (`-h` も同じ) は使い方を標準出力に出して 0 で終わり、サーバも窓も始めない。受け付ける
オプションは `--headless` と `--help` / `-h` だけで、それ以外 (`--no-such-option`、位置引数) は名指しのエラーを
出して 1 で終わる (`--help` と一緒に書いてもエラーが優先)。文面の正本は `tools/studio/studio_settings.cpp` の
`studio_usage()` (Linux での出力。Windows では区切り `;` と `USERPROFILE` になる):

```text
usage: ergo_studio [--headless]
       ergo_studio --help | -h

Ergo Studio: the loopback HTTP / WebSocket server the engine clients and tools talk to,
plus its window. Stop it with Ctrl+C (SIGINT).

options:
  --headless   server only, no window (a build without the window is always headless)
  --help, -h   print this text and exit (nothing is started)

environment (unset or empty = the default):
  PORT                     port to listen on, 1..65535 (default 5170)
  ERGO_HOST                address to bind (default 127.0.0.1)
  ERGO_PLUGIN_DIR          external plugin pack directories (separated by ':')
  ERGO_STUDIO_HOME         plugin ledger and fetched plugins (default <home>/.ergo-studio)
  LUDIARS_ROOT             where the Excubitor catalog and the Revisor CLI are (studio plugin)
  EXCUBITOR_URL            Excubitor URL (default: from the catalog)
  ERGO_ACTIO_TOKEN         Actio bearer token (unset = Actio local mode)
  ERGO_CURARE_URL          a remote Curare server (default: from the Excubitor topology)
  ERGO_CURARE_TOKEN        Curare bearer token
  ERGO_PLUGIN_OWNERS       GitHub owners plugins may be fetched from, comma list (default LUDIARS)
  ERGO_STUDIO_TIMEOUT_MS   limit of one service call in ms (default 8000)
  VISUS_PROJECT_ROOTS      project roots the visus plugin scans (';' separated)
  ERGO_UI_LAYOUT_ROOT      file root of the ui_layout plugin (unset = not configured)
  ERGO_PICTOR_PROFILE_DIR  Pictor *.profile.json directory (render_pipeline plugin)
  ERGO_VFX_BACKEND         vfx_suggest / vfx_breed backend (default cli, the only one)
  ERGO_VFX_MODEL           model name passed to the claude CLI (default haiku)
  CLAUDE_BIN               claude CLI to run (default claude)
  ERGO_STUDIO_FONT         window font (default <WINDIR>\Fonts\meiryo.ttc when WINDIR is set)
  WINDIR                   Windows directory, for the default font
  HOME                     home directory (melpomene / terpsichore / thaleia, studio home)

At start-up the log prints the server address and each plugin with its path; plugins
that clients reach directly add one line saying how (e.g. the variable WebSocket).
```

```text
$ ergo_studio --no-such-option; echo $?
[err] [ergo] unknown option "--no-such-option" (ergo_studio --help lists the options)
1
```

起動したときのログには、サーバの番地とプラグインの一覧に加えて、窓なしで使う入口が 1 行ずつ出る (プラグインの
`PluginInfo::usage_hint`。`{addr}` が `<host>:<port>` になる):

```text
[info] [ergo] http://127.0.0.1:5491/
[info] [ergo]   -> Ergo Studio  (/studio/)
[info] [ergo]   -> Variable Editor  (/variable/, ws)
[info] [ergo]        games and clients: ws://127.0.0.1:5491/variable/ws (first frame {"op":"hello","role":"engine"|"ui"})
...
[info] [ergo]   -> 授業  (/lesson/)
[info] [ergo]        without the window: POST http://127.0.0.1:5491/lesson/api/connect, JSON body {"link":"<connection string>"}
```

授業につなぐ: `curl -X POST -H 'Content-Type: application/json' -d '{"link":"<接続文字列>"}' http://127.0.0.1:<port>/lesson/api/connect`
(`Content-Type` が JSON でなければ `403 forbidden`、上の LS-INV-06)。結果は `GET /lesson/api/state` で見る。

変数を切り替える (`/variable/ws` の role "ui" のクライアント。`tools/studio/plugins/variable/variable_plugin.cpp`):

1. 送る `{"op":"hello","role":"ui"}` → 届く `{"op":"actors","nodes":[...]}` と `{"op":"registry","vars":[...]}`
   (`vars` の各要素は `{app, kind, meta, name, value}`)。
2. 送る `{"op":"set","name":"feel.hit_stop","value":true}` → Studio がその変数を持つゲームへ渡し、ゲームが値を変えると
   全 ui クライアントに `{"op":"value","name":"feel.hit_stop","value":true}` が届く。登録されていない名前の `set` は
   何も起こさない (返事も無い)。

実際に行った手順と出力 (2026-11-12、この Linux、`cmake -G Ninja` の build、`NO_PROXY=127.0.0.1,localhost`、
Node 22 の `WebSocket` で書いた使い捨てのクライアント `ws_switch.mjs`: hello を送り、`registry` が来たら `set` を送り、
`value` が来たら閉じる。ゲームは自分の値が変わるたびに `value` を送るので、`set` の返事の前に観測値の `value` も届く):

```text
$ PORT=5491 <build>/studio/ergo_studio --headless &
$ <build>/examples/ergo_example_lesson_action_feel --port 5491 --plain --status-every 1 &
$ node ws_switch.mjs ws://127.0.0.1:5491/variable/ws feel.hit_stop true
> {"op":"hello","role":"ui"}
< {"nodes":[],"op":"actors"}
< {"op":"registry","vars":[{"app":"lesson_action_feel","kind":"bool","meta":{"category":"feel"},"name":"feel.camera_shake","value":false},{"app":"lesson_action_feel","kind":"bool","meta":{"category":"feel"},"name":"feel.hit_stop","value":false}, ... (27 in all)]}
> {"op":"set","name":"feel.hit_stop","value":true}
< {"name":"game.score","op":"value","value":98}
< {"name":"game.player_hits","op":"value","value":4}
< {"name":"combat.enemy.flinches","op":"value","value":4}
< {"name":"feel.hit_stop","op":"value","value":true}
```

そのときのサンプルの出力:

```text
[lesson_action_feel] started (--plain): hit_stop=OFF knockback=OFF camera_shake=OFF; enemy flinch=ON super_armor=off motion_cancel=OFF; player flinch=ON super_armor=off motion_cancel=ON
[lesson_action_feel] Studio ws://127.0.0.1:5491/variable/ws
[lesson_action_feel] waiting for the Studio (variable plugin)
[lesson_action_feel] connected to the Studio
[lesson_action_feel] real=1.0s world=1.0s hits=0 defeated=0 wave=1 hp=100 hit_stop=OFF knockback=OFF camera_shake=OFF super_armor=off hit_stop_last_seconds=0.000 knockback_last_impulse=0.000 knockback_travel=0.00 shake=0.000
[lesson_action_feel] combat enemy: flinches=0 interrupted=0 armor_absorbed=0 cancels=0 | player: flinches=0 interrupted=0 armor_absorbed=0 cancels=0
[lesson_action_feel] feel.hit_stop: OFF -> ON
[lesson_action_feel] real=2.0s world=1.9s hits=5 defeated=1 wave=1 hp=100 hit_stop=ON knockback=OFF camera_shake=OFF super_armor=off hit_stop_last_seconds=0.083 knockback_last_impulse=0.000 knockback_travel=0.00 shake=0.000
[lesson_action_feel] combat enemy: flinches=4 interrupted=1 armor_absorbed=0 cancels=0 | player: flinches=0 interrupted=0 armor_absorbed=0 cancels=5
```

`ergo_example_lesson_action_feel --help` の出力 (正本は `examples/lesson_action_feel/options.cpp` の `kUsage`):

```text
usage: ergo_example_lesson_action_feel [--auto] [--host H] [--port P] [--no-studio] [--plain]
                                       [--seconds S] [--status-every S] [combat options]
       ergo_example_lesson_action_feel --play [--endless | --autopilot] [--host H] [--port P] [--no-studio] [--plain]
                                       [--seconds S] [combat options]
       ergo_example_lesson_action_feel --check
       ergo_example_lesson_action_feel --help | -h

Arena action game for the Odeum lesson. Publishes feel.hit_stop, feel.knockback,
feel.camera_shake, feel.super_armor and the combat.* settings (writable) and what really
happened (feel.hit_stop_last_seconds, feel.knockback_last_impulse, combat.*.flinches, ...)
to the Studio's variable plugin at ws://H:P/variable/ws.

modes:
  --auto             (default) headless: a scripted player fights endless waves; status lines on stdout
  --play             play it in this terminal (POSIX terminals; keys are shown on screen)
  --check            offline self-check without a Studio (exit 0 = the observations follow the switches)
  --help, -h         print this text and exit

options:
  --host H           Studio host (default 127.0.0.1)
  --port P           Studio port, 1..65535 (default 5170; the Studio's PORT)
  --no-studio        do not connect to a Studio
  --plain            start with hit stop, knockback and camera shake OFF
  --endless          --play: waves never end (default: clear 3 waves to win)
  --autopilot        --play: the scripted player plays endless waves on screen (for a demo);
                     keys 1-9, P, R and Q still work
  --seconds S        stop after S seconds of real time (default 0 = until stopped)
  --status-every S   --auto: status lines every S seconds, 0.1..3600 (default 2)

combat options (per side; the same switches as keys 4-9 and the combat.* variables):
  --enemy-flinch on|off              default on
  --enemy-super-armor off|attack|always   default off (attack = during wind-up and swing)
  --super-armor                      same as --enemy-super-armor attack
  --enemy-cancel on|off              motion cancel on hit, default off
  --player-flinch on|off             default on
  --player-super-armor off|attack|always  default off
  --player-cancel on|off             default on
  --knockback-dir away|backward      away from the attacker (default) or the target's backward direction
```

## 未決・今後

- 文字入力欄が UI に入ったら、接続文字列を欄に貼れるようにする (今はクリップボードのボタンだけ)。
- 条件の判定はポーリングの時点の値で行う。ポーリングの間だけ満たされる値は見逃すので、サンプル側は
  `..._last_seconds` や数え上げ (`combat.enemy.armor_absorbed`) のように残る値を公開する (`examples/lesson_action_feel` はそうしている)。
- `int64` は JSON の数値として届くので、2^53 を超える値の比較は正確でない。
- 読み取り専用 (`read_only`) の変数は「従っていない」として出る。読み取り専用だから、と理由までは区別しない
  (`IGameVariables` は kind と値だけを渡す)。
