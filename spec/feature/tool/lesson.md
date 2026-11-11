# 授業連携 (`lesson` プラグイン) {#SPEC-ergo-lesson}

依頼 (2026-11-11): odeum-lesson (LUDIARS/Odeum、AI が進める授業) と Ergo Studio をつなぐ。生徒は
ブラウザで授業に参加し、手元では Ergo Studio と、`ergo_bind` (`/variable/ws`) で Studio の `variable`
プラグインにつないだゲームを動かしている。Studio は次の 2 つを自動で行う。

- **生徒**: 動かしているゲームが公開する変数が課題の手順の条件 (`check`) を満たしたら、その手順の達成を
  odeum-lesson に報告する。
- **実演** (授業で実演を映している教員の PC): 授業の進行が実演の合図 (`demo`) に達したら、ゲームの機能の
  ON / OFF (bool 変数) を切り替える。

## 置き場所

ネイティブ版 Ergo Studio (`tools/studio`) の組み込みプラグイン。Studio 自身のサーバには HTTP / WS の
経路を足さない (`IPluginHealth` と `IPluginView` だけを持つ)。

| 層 | ファイル | 責務 |
|---|---|---|
| 接続文字列 (純関数) | `plugins/lesson/lesson_link` | 接続文字列の検査と分解 |
| 取り決め (純関数) | `plugins/lesson/lesson_protocol` | pair / state / progress の JSON ⇄ 構造体、失敗の分類 |
| 通信 | `plugins/lesson/lesson_client` | `platform::IHttpClient` での 3 つの呼び出し |
| 判定 (純関数) | `plugins/lesson/step_check` | 手順の条件と変数の値の比較 |
| 実演 | `plugins/lesson/demo_apply` | どの切り替えが済み、どれがゲーム待ちかの管理 |
| 状態機械 | `plugins/lesson/lesson_session` | 「1 回ポーリングする」で進む。スレッドも時計も持たない |
| プラグイン | `plugins/lesson/lesson_plugin` | ワーカースレッド、表示用の状態、health |
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

`demo.seq` が前に反映した値と違ったら、`demo.states` の各機能について、`features` にある変数へ bool の
設定を `variable` プラグイン経由で頼む。

- 変数が公開されていない (ゲームが未接続、名前が無い) ものは「未反映」として覚え、公開されるか、次の
  `seq` に置き換えられるまで、ポーリングのたびに試す。画面にどの機能がゲーム待ちかを出す。
- 変数が bool でなければ設定せず、「bool ではありません」と出す (これも毎回見直す)。
- `features` に無い機能は「対応する変数がありません」と出す (黙って捨てない)。
- 「反映」は設定の依頼がゲームへ送られたことを指す。ゲームが受け入れたかの確認はしていない。

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

## 不変条件

### LS-INV-01 秘密はメモリだけ {#SPEC-ergo-lesson-secrets}

token と接続コードはメモリにだけ持つ。ディスクに書かず、ログに出さず、画面にも health にも出さない。
画面に出すのは接続先のホスト名だけ (ポートも出さない)。サーバの `error` は英小文字・数字・`_` の
40 文字以内のときだけ表示する。

### LS-INV-02 進み具合は戻さない {#SPEC-ergo-lesson-latched}

`done:false` は送らない。一度報告した手順の印を Studio から外すこともしない。

### LS-INV-03 代わりの経路を持たない {#SPEC-ergo-lesson-no-fallback}

通信は `platform::IHttpClient` だけ (Windows は WinHTTP)。実装の無い環境では「未対応」の失敗をそのまま
画面に出し、別の経路に切り替えない。クリップボードも同じ。取り決めと違う応答は既定値で埋めずに失敗にする。

### LS-INV-04 通信は UI スレッドの外 {#SPEC-ergo-lesson-thread}

HTTP はワーカースレッド 1 本だけが行う。UI スレッドは指示を積み、最後に公開された表示用の状態を読むだけ。
プラグインの破棄でワーカーを止めて join する (呼び出し中なら、その上限時間 5 秒まで待つ)。

### LS-INV-05 接続先 {#SPEC-ergo-lesson-origin}

接続先は貼り付けた接続文字列の origin だけ。設定・環境変数・ポート番号をコードに持たない。

## 検証

- `tests/studio/test_studio_lesson.cpp`: 接続文字列の受理 / 拒否の表、取り決めの読み取り (壊れた JSON は
  明示の失敗)、条件の表 (全演算子・型の不一致・変数なし)、生徒の流れ (1 回だけの POST と本文・Bearer、
  サーバで済みの手順は送らない、送信失敗の送り直し、409 で停止、401 で切断)、実演の流れ (seq の変化で反映、
  未公開の変数は保留して公開後に反映、対応の無い機能の報告)、`variable` プラグインとの橋渡し、プラグインの
  health とログに秘密が出ないこと。
- `tests/studio/test_studio_lesson_view.cpp`: タブの文言 (未接続、貼り付けの拒否理由、生徒、実演、失敗の種類)
  と、画面に token・コード・貼り付けた文字列が出ないこと。
- 確認していないこと (2026-11-11 時点):
  - 実際の odeum-lesson との接続 (テストは `tests/studio/fake_http.h` の偽の HTTP だけ)。
  - Windows でのビルドと動作。WinHTTP 経由の通信と `win_clipboard.cpp` は Linux ではコンパイルもしていない。
  - 窓のある Studio での表示 (Pictor + Vulkan + GLFW が要る)。`✓` などの字がフォントにあるかも未確認。
  - 実際のゲームの変数。**ActionGameSample の Ergo 版はまだ無い**。Odeum のサンプル講座が使う `feel.*`
    (`feel.hit_stop`、`feel.hit_stop_last_seconds` など) は取り決めた名前であり、これから作るサンプルが
    `ergo_bind` で同じ名前を公開する必要がある。

## 未決・今後

- 文字入力欄が UI に入ったら、接続文字列を欄に貼れるようにする (今はクリップボードのボタンだけ)。
- 条件の判定はポーリングの時点の値で行う。ポーリングの間だけ満たされる値は見逃すので、サンプル側は
  `..._last_seconds` のように残る値を公開する。
- 実演中にゲームを起動し直すと、変数は既定値に戻る。反映し直すのは次の実演の合図 (`seq` の変化) のとき。
- `int64` は JSON の数値として届くので、2^53 を超える値の比較は正確でない。
- 読み取り専用 (`read_only`) の変数への設定は、ゲームが黙って無視する。Studio からは区別していない。
