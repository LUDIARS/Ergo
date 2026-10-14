# ergo_pkg watch の報告・出来事・前回状態 (schema 1)

- 正本の設計: [`spec/feature/package-manager.md`](../feature/package-manager.md) §2.3 / §3 / §4 / §8
- 書く側: `ergo_pkg watch` / `outdated` (`ergo_package_watch`、 [`spec/module/package_watch.md`](../module/package_watch.md))
- 読む側: Excubitor の配送ジョブ (`events.json`、 W5)、 Unity パッケージ (`watch-report.json`、 W7)、 P4 ホスト状態図 (W6)

Unity とネイティブで共有するのは **この形式** と pin の規則 (40 桁 SHA、 タグは注釈) だけで、 判定は C++ にしか無い。
JSON はキーを辞書順に並べた 1 行 (改行 1 つで終わる)。 同じ入力なら同じバイト列。

## 載せないもの (§8)

パス・URL・ポート・トークン・セッション id・PR のタイトルと本文・ブランチ名 (委託のタスク名を含み得る)。 local PR は番号と head の SHA7 だけ。 取得元の URL (`repository`) は報告・出来事のどこにも入れない。 非公開パッケージは
`private: true` を付ける (名前とコードは載る。 非公開ホストの出来事は配送側が別の webhook に送る)。 書く前に
ergo_package_manager の漏れ検査 (ドライブ文字始まり・`/Users/`・`/home/`・UNC・`token` / `secret` / `authorization` のキー) を
全ての報告と `events.json` に当て、 1 件でも当たれば **何も書かずに** 終了 3。

## 更新 1 件 (candidate)

報告の `updates` / `events` / `pending` と前回状態の `entries` で共通の形。

| キー | 型 | 意味 |
|---|---|---|
| `dependency` | 文字列 | 依存の名前 (`ergo` は Ergo 自身) |
| `kind` | `"tag"` / `"track"` / `"off-main"` / `"run"` / `"review"` / `"merged"` | 種類 (§3)。 run / review / merged は W4 (`watch` だけが作る) |
| `subject` | 文字列 | 重複判定の鍵。 tag はタグ名、 track は追跡ブランチの先頭の SHA、 off-main は pin の SHA、 run は run id、 review は `#<番号>@<head SHA7>`、 merged は `#<番号>` |
| `from_rev` / `from_tag` | 文字列 | 今の pin と `expect_tag` |
| `to_rev` / `to_tag` | 文字列 | 上げ先 (40 桁 SHA) とそのタグ (tag だけ)。 off-main の `to_rev` は追跡ブランチの先頭 |
| `behind` | 数 / null | pin..上げ先 のコミット数。 ミラーも兄弟チェックアウトも無ければ null |
| `warning` | 真偽 | off-main |
| `strict_ok` | 真偽 | 上げ先がタグで STRICT で通る |
| `next` | 文字列 | 次の操作 (`ergo_pkg update <name> --to <tag\|main\|sha>`)。 `ergo_pkg` は実行しない。 持ち主のリポで feat ブランチ + PR |

run / review / merged だけが持つキー (tag / track / off-main の形は W3 のまま変えない):

| キー | 型 | 意味 |
|---|---|---|
| `origin` | `"pf"` / `"ai"` / `"human"` / `"unknown"` | 由来 (下の判定) |
| `trigger` | 文字列 | ai のとき run の `triggered_by` の写し (それ以外は `""`) |
| `review` | 数 / null | review / merged: local PR の番号 (run は null) |
| `head` | 文字列 | review / merged: local PR の head の SHA7 |

merged の `to_rev` は main に入った commit (`mergeCommitSha`、 40 桁でなければ `""`)。 run / review の `to_rev` は `""`。

判定:

- **tag**: 正式版 (pre-release でない) の `v*` タグを semver の新しい順に見て、 `expect_tag` より新しく、 pin の子孫で pin と違う
  最初の 1 つ。 履歴 (ミラー / 兄弟) が答えないときは `expect_tag` より新しい最初の 1 つ (`expect_tag` が無ければ出さない)
- **track**: 追跡ブランチ (`track.branch`、 既定 `main`) の先頭が pin と違い、 pin がその祖先。 祖先かどうか答えられないときも
  先頭が動いたことは出す (`behind` は null)
- **off-main**: pin が追跡ブランチの先頭の祖先でない (squash で消える feature の SHA など)。 track とは同時に出ない
- **run** (W4): 依存のリポを対象にした Concordia の委託 run が進行中 (`queued` / `launching` / `pending` / `spawned` / `running` / `blocked`)。
  対象リポは run の `sessions[].repo_origin`、 無ければ `args.target_repo` の末尾名で比べる (大文字小文字を区別しない)
- **review** (W4): 依存のリポで base が追跡ブランチの open な Revisor local PR
- **merged** (W4): 前回状態で review だった local PR が merged の一覧にある。 由来と head は前回の review から写す。 1 回だけ出し、 状態には残さない
- **由来**: PR の `sessionId` が run の `child_session_id` と一致 (無ければ `headRef == spawn_branch` かつ同じリポ) する run があれば、
  その `triggered_by` が `praeforma` なら `pf`、 それ以外なら `ai`。 Concordia に届いて突合する run が無ければ `human`、
  Concordia に届かない・認可されない・URL が無ければ `unknown`。 run の由来は `triggered_by` だけで決める (pf / ai)

## `<host>/.ergo/watch-report.json` (ホストごと)

`<host>/.ergo/` は `ergo_pkg init` が `.gitignore` に足す (追跡しない)。

| キー | 型 | 意味 |
|---|---|---|
| `schema` | `1` | |
| `host` | 文字列 | ホストのコード (`host.code`、 Ergo 自身は `Ergo`) |
| `generated_at` | 文字列 | `YYYY-MM-DDTHH:MM:SSZ` (UTC) |
| `dependencies` | 配列 | 依存ごとの今回の観測 (名前順、 下表) |
| `events` | 配列 | 今回の新しい出来事 (`events.json` の 1 件と同じ形) |
| `pending` | 配列 | 既知の未解消 (前回状態の `entries` に `id` を足したもの)。 日次要約に使う |
| `sources` | `{ concordia, revisor }` | W4: `"ok"` / `"unavailable"` (届かない・HTTP 200 以外・形が違う) / `"off"` (URL が渡されていない)。 `unavailable` の取得元の種類は「更新なし」ではない |

`dependencies[]`:

| キー | 型 | 意味 |
|---|---|---|
| `name` / `code` / `owner` / `private` | | 依存の名前・コード・pin の持ち主 (`host`: Ergo 自身と非公開、 `ergo`: 共有パッケージ。 Ergo のマニフェストを直す)・非公開か |
| `observed` | 真偽 | **false なら「更新なし」ではない**。 その回は前回状態を書き換えない |
| `reason` | 文字列 | observed が false の理由 (`git ls-remote failed (offline or not reachable)`、 `track branch 'x' not found` など) |
| `as_of` | `"ls-remote"` / `"sibling-last-fetch"` / `""` | 値の時点。 兄弟チェックアウトは最後に fetch した時点 |
| `pin` | `{ rev, tag }` | 今の pin と `expect_tag` |
| `track` | `{ branch, head, behind, on_track }` | 追跡ブランチ・先頭・pin..先頭の距離・pin が先頭の祖先か (null は不明) |
| `sibling_behind` | 数 / null | 兄弟チェックアウトの HEAD..追跡ブランチ (兄弟チェックアウト自体の古さ) |
| `updates` | 配列 | 今そこにある更新 (前回状態を見ない。 `outdated` と同じ) |

## `<out>/events.json` (全ホスト、 配送ジョブが読む)

| キー | 型 | 意味 |
|---|---|---|
| `schema` | `1` | |
| `generated_at` | 文字列 | UTC |
| `hosts` | 文字列配列 | 今回見たホスト (コード順) |
| `events` | 配列 | 新しい出来事 (ホスト → 依存 → kind → 主体 の順) |
| `sources` | `{ concordia, revisor }` | 報告と同じ (今回の watch で共通) |

出来事 1 件 = 更新 1 件 に次を足したもの:

| キー | 型 | 意味 |
|---|---|---|
| `id` | 文字列 | `<host>/<依存>/<kind>/<主体>`。 Unity の既読管理の鍵 |
| `host` | 文字列 | ホストのコード |
| `code` / `owner` / `private` | | 依存のコード・持ち主・非公開か |
| `as_of` | 文字列 | 値の時点 (run / review / merged は `""`) |
| `at` | 文字列 | 出来事にした時刻 (UTC) |
| `since_notified` | 数 / null | track: 前回の通知からの距離の差 (6 時間の畳み込みの合算) |

## 前回状態 `<state>/<code>.watch-state.json`

リポの外 (Excubitor のデータディレクトリ、 または利用者ディレクトリ) にだけ置く。 消すと次の回は全てを新規として出す。

| キー | 型 | 意味 |
|---|---|---|
| `schema` | `1` | 違えば `watch` は終了 2 (消して作り直す) |
| `host` | 文字列 | ホストのコード |
| `fingerprints` | オブジェクト | 依存名 → ref 一覧の指紋 (16 桁の 16 進)。 同じならミラーを同期しない |
| `entries` | 配列 | 既知の `(依存, kind, 主体)`: 更新 1 件 + `first_seen` / `last_notified` (UTC) + `notified_behind` (track: 最後に通知したときの距離) |

重複の抑止 (§3):

- 同じ `(host, 依存, kind, 主体)` は 1 回だけ出来事にする。 既知は `entries` に残る (`pending`)
- track は主体 (先頭の SHA) が進むたびに変わるので、 同じ依存では最後の通知から 6 時間は出さずに `entries` の主体だけ替える。
  6 時間を過ぎて出すときは `since_notified` に最後の通知からの距離の差を添える
- 今回の判定に無いもの (pin を上げて解消したもの) は消す。 再発は新規
- 観測できなかった依存は、 前回の `entries` と指紋をそのまま残す。 宣言から消えた依存は消す
- W4: 取得元ごとに見る。 Revisor に届かなければ review の既知を、 Concordia に届かなければ run の既知を前回のまま残す
  (ref が取れなかったときの tag / track / off-main と同じ扱い)。 終わった run と閉じた review は消す
