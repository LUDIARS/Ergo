# log plugin — 動作中のログを見る

## 概要

Ergo 統合開発者ツール (`tools/ergo`) の組み込みプラグインの 1 つ。
エンジン (ホストアプリ) が `ergo_log` で出した行を WebSocket で受け取り、
アプリごとに直近 10k 行をリングに保持してブラウザに表示する。 UI では
アプリを選び、 レベル・文字列・フレーム範囲で絞り込み、 自動スクロールと
一時停止ができる。 GUI アプリや Electron / Custos 経由の実行のように
コンソールが見えない場合でも、 動かしながらログで詰まりを探せる。

リポジトリ内のエンジン側クライアントは `ergo_log_live` の `LiveSink`
(`include/ergo/log/live_sink.h`、 [`spec/feature/module/log.md`](../module/log.md))。
ホストは `add_sink(&live)` と `live.start()` を呼ぶだけでよく、 ソケットや
JSON のコードは書かない。

## 設置場所

- 実装: `tools/ergo/src/plugins/log/`
  - `index.ts` — plugin definition (Hono routes + WS 中継)
  - `protocol.ts` — wire protocol の型、 受信フレームのガード
    (`sanitizeLine` / `sanitizeLinesMsg`)、 コンソール形式のパーサ
    (`parseConsoleLine`)
  - `ring.ts` — アプリごとの行リング (`LineRing`) とフィルタ
  - `ui/index.html` / `ui/styles.css` / `ui/app.js` — UI 本体 (ES module)
- 登録: `tools/ergo/src/core/registry.ts` の `PLUGIN_FACTORIES`
- テスト: `tools/ergo/tests/plugins/log/` (`ring` / `protocol` / `relay`)

## URL

| URL | 役割 |
|---|---|
| `http://localhost:5170/log/` | UI |
| `http://localhost:5170/log/api/apps` | 保持しているアプリの一覧 (`{ apps: AppInfo[] }`) |
| `http://localhost:5170/log/api/lines?app=...` | アプリの行 (下記) |
| `http://localhost:5170/log/api/health` | プラグインヘルス (`apps` / `engines` / `uis`) |
| `ws://localhost:5170/log/ws` | エンジン / UI 共通の WS 入口 |

### `GET /log/api/lines`

| パラメータ | 意味 |
|---|---|
| `app` | 必須。 アプリ名 (`/log/api/apps` の `app`)。 無ければ 400、 未知なら 404 |
| `level` | カンマ区切りのレベル (`ERR,WRN`。 大小文字は問わない)。 省略で全レベル |
| `since` | この `seq` より新しい行だけ |
| `q` | 本文の部分一致 (大小文字を区別しない) |
| `frame_min` / `frame_max` | フレーム範囲 (両端を含む) |
| `limit` | 最大件数 (既定 1000、 上限 10000)。 超えたら **新しい方**を返す |

応答は `{ app, last_seq, dropped, lines: StoredLine[] }` (古い順)。
`last_seq` を次の `since` に渡せば新着だけを取れる。 不正な値は 400。

## プロトコル (WS)

すべて JSON テキストフレーム。 接続の役割は最初の `hello` で決まる。

### エンジン → サーバ

```json
{ "op": "hello", "role": "engine", "app": "mygame" }
```

```json
{ "op": "lines", "app": "mygame", "dropped": 0,
  "lines": [ { "lvl": "INF", "frame": 1234, "ts_us": 20581234,
               "tid": 140235, "msg": "player connected" } ] }
```

- `lvl` は `ERR` / `WRN` / `INF` / `DBG` (ergo_log のコンソール表記と同じ)
- `frame` はフレームプロバイダ、 `ts_us` は時刻プロバイダ (µs) の値。
  ホストが `ergo::profile::now_us` をつないでいれば `ergo_profile` の
  タイムラインと同じ時間軸になる
- `tid` は `std::hash<std::thread::id>` (`ergo_profile` の tid と同じ値)。
  JSON の数値なので 2^53 を超える下位ビットは丸まる
- `dropped` は **前のフレーム以降に** エンジンが捨てた行数 (LiveSink の
  キューがあふれた分)
- `ergo_log_live` は jsonm で書くのでキーは辞書順になる。 サーバは順序に
  依存しない
- 形の崩れた行 (未知のレベル、 数値でない `frame` / `ts_us` / `tid`、
  文字列でない `msg`) はその行だけ捨てる。 `lines` が配列でないフレームは
  丸ごと捨てる
- 再接続直後は `lines` が `hello` より先に届くことがある。 役割の決まって
  いない接続の `lines` は、 そのフレームの `app` を名前にしたエンジンとして
  登録する。 以後の `hello` は無視する

### UI → サーバ

```json
{ "op": "hello", "role": "ui" }
```

UI の接続が送った `lines` は無視する。

### サーバ → UI

```json
{ "op": "apps", "apps": [ { "app": "mygame", "connected": true,
                             "lines": 812, "last_seq": 812, "dropped": 0 } ] }
```

```json
{ "op": "backlog", "app": "mygame", "dropped": 0, "lines": [ <StoredLine>, ... ] }
```

```json
{ "op": "lines", "app": "mygame", "dropped": 0, "lines": [ <StoredLine>, ... ] }
```

- `hello` の直後に `apps` と、 アプリごとの `backlog` (リング全体) を送る。
  `backlog.dropped` はそのアプリの累計
- 以後、 エンジンの `lines` を受けるたびに保存した行を `lines` で中継する。
  こちらの `dropped` はそのフレームで報告された分 (差分)
- エンジンの接続・切断、 アプリの追い出しのたびに `apps` を送り直す
- `StoredLine` はエンジンの行に `seq` を足したもの。 `seq` はアプリごとに
  1, 2, 3, ... と振り、 エンジンが再接続しても続きから振る

## アプリ名とリング

- アプリごとに直近 **10,000 行**のリングを持つ。 あふれたら古い行から消える
- 同じ名前のエンジンが同時に 2 つ以上つながると、 後から来た方を
  `mygame#2`、 `mygame#3`、 ... と呼ぶ
- エンジンが切断してもリングは残る (クラッシュした実行のログを後から
  読める)。 同じ名前のエンジンがつながると、 そのリングを引き継ぐ
- リングは最大 **16 個**。 超えたら、 エンジンが切断済みのアプリのうち
  最後に動きがあったのが最も古いものから消す。 接続中のアプリは消さない

## UI

- アプリ選択 (切断済みのアプリは `(disconnected)` 付き)
- レベルのチェックボックス (各レベルの行数付き)、 本文の文字列フィルタ、
  フレーム範囲 (frame 列をクリックするとそのフレームだけに絞る。 もう一度
  クリックで解除)
- 自動スクロール、 一時停止 (止めている間に届いた行数を表示し、 再開で
  描き直す)
- 列: frame / time (s) / lvl / thread (アプリ内で `T1`、 `T2`、 ... と短く
  表示。 ホバーで tid) / message
- ブラウザはサーバと同じ 10k 行を持ち、 表示は条件に合う新しい 2,000 行まで

## コンソール形式

`protocol.ts` の `parseConsoleLine(text)` は、 ergo_log のコンソール出力
`[F%08llu][TAG] msg` の 1 行を `{ lvl, frame, msg }` にする (行末の
`\n` / `\r\n` / `\r` は無視、 それ以外の形式は null)。 ツールが起動した
プロセスの標準出力を同じモデルに流し込む将来の runner (ergo-redesign の
`apps` plugin) 向けで、 このプラグイン自身は使っていない。

## 既知の制限

- ツール本体 (`tools/ergo` の core) のログ (`PluginContext.log`) はこの
  プラグインに流さない (core がプラグインを import することになるため)
- 行の削除・保存・エクスポートは無い。 UI の表示はブラウザ側の絞り込み
  だけで、 サーバ側の購読フィルタは無い
- 時刻はエンジンの時計なので、 別プロセスのアプリ同士の `ts_us` は
  比べられない
