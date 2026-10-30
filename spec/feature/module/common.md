# Common モジュール定義

## 概要

`ergo_common` / `ergo_common_ws` は、 複数のモジュールが共有する **ドメインを
持たない小さな部品**の置き場。 モジュール同士 (`ergo_<domain>`) は互いを
import しない規約なので、 2 つ以上のモジュールが同じ仕組みを必要とした
ときは、 ここ (下層) に下ろして共有する。

| ターゲット | 中身 | ヘッダ |
|---|---|---|
| `ergo_common` | 最小 JSON コーデック (`ergo::common::jsonm`) | `include/ergo/common/json_min.h` |
| `ergo_common_ws` | 共有 WebSocket クライアント (`ergo::common::ws::Client`) | `include/ergo/common/ws_client.h` |

2 つに分けているのは、 JSON だけ使うモジュール (シーン文書、 パッケージ
マニフェスト等) にソケットとスレッドを持ち込まないため。 `ergo_common` は
ソケットに依存しない。

**ここに置いてよいのは、 特定のドメイン (描画・サウンド・シーン・ログ…) を
知らない汎用ユーティリティだけ。** あるモジュールのプロトコルやデータ型
(例: `bind` の `hello` / `bind` メッセージ、 シーンのパッチ) は各モジュール
側に置き、 ここには「JSON を読み書きする」 「WS でテキストを送受信する」
といった土台だけを置く。 単一のモジュールしか使わないものは、 そのモジュール
の中に置く。

## カテゴリ
システム

## 所属ドメイン
基盤 (foundation)

## 依存
- `ergo_common`: C++17 標準ライブラリのみ
- `ergo_common_ws`: C++17 標準ライブラリ、 Threads (`std::thread`)、
  OS ソケット (BSD sockets / Winsock。 Windows では `ws2_32` をリンク)
- (テスト) GoogleTest 互換 mini-gtest

## jsonm — 最小 JSON コーデック

`parse(src, out)` / `serialize(value)` と、 値型 `JsonValue`
(`Null` / `Bool` / `Number` / `String` / `Array` / `Object`)。 WS の小さな
RPC ペイロードや設定ファイル程度を想定する。

- 数値はすべて `double`。 整数は 2^53 を超えると精度が落ちる。 書き出しは
  `%.17g`
- オブジェクトは `std::map` なので **キーは辞書順**に書き出される (同じ値
  なら同じテキストになる)。 重複キーは後勝ち
- 文字列の `\uXXXX` は UTF-8 に変換する (サロゲートペアは結合、 単独の
  サロゲートはそのまま 3 バイト形式)。 書き出しは制御文字だけをエスケープし、
  非 ASCII はそのまま出す
- 入れ子は 64 段まで。 末尾に余計なトークンがある入力は不正として扱う。
  不正な入力で落ちることはなく、 `parse` が false を返す
- ストリーミング、 スキーマ検証、 コメント・末尾カンマなどの拡張構文は
  持たない

## ws_client — 共有 WebSocket クライアント

ツリー内で唯一の C++ WebSocket クライアント。 エンジン側から `tools/ergo`
の各プラグインへ **外向きに**接続するために使う (サーバ機能は持たない)。

```cpp
namespace ws = ergo::common::ws;
ws::Client client;
client.set_on_open([&] { client.send_text(hello); });   // 再接続のたびに呼ばれる
client.set_on_text([&](const std::string& frame) { /* ワーカースレッド */ });
client.start("127.0.0.1", 5170, "/variable/ws");
...
client.stop();
```

- **スレッド**: 1 本のワーカースレッドが接続・送信・受信を担う。
  `on_open` / `on_text` は **ワーカースレッドで**呼ばれるので、 ホストの
  状態に触れる処理はキュー等でメインスレッドへ渡す。 `send_text` は
  どのスレッドからでも呼べる
- **切断中の送信は捨てる**: `send_text` は未接続なら何もしない。 相手側に
  状態を持たせるプロトコル (`hello` + 登録の再送など) は `on_open` で
  送り直す。 `on_open` はハンドシェイク成功のたびに (再接続を含めて) 呼ばれる
- **再接続**: 接続失敗・ハンドシェイク失敗・相手が TCP を閉じた・不正な
  フレームを受けたときは約 1.5 秒待って再接続する。 サーバから close
  フレームを受けたときは待たずに再接続する。 `stop()` は接続待ち (connect)
  中でも、 バックオフ中でも、 ハンドシェイク待ちでも、 相手が読まなくなって
  送信が詰まっていても速やかに戻る (接続は最大約 5 秒、 ハンドシェイクは
  最大約 2 秒で諦める)
- **フレーム上限**: 受信フレームのヘッダが 1 MiB を超える長さを宣言したら、
  ペイロードを待たずに接続を落とす (その後は通常どおり再接続)
- 101 応答と同じ受信に含まれていた最初のフレームも取りこぼさない
  (接続直後に状態を送ってくるサーバ向け)
- 対応するのはテキストフレームと close のみ。 ping / pong / バイナリ /
  分割フレームは読み捨てる。 TLS (`wss://`) と `Sec-WebSocket-Accept` の
  検証は行わない (ローカルの開発ツール接続用)
- 送信できなくなった相手 (切断済み) へ書いてもプロセスは SIGPIPE で
  落ちない (Linux は `MSG_NOSIGNAL`、 macOS は `SO_NOSIGPIPE`)

### 利用者

| モジュール | ターゲット | 接続先プラグイン |
|---|---|---|
| bind | `ergo_bind` | `variable` (`/variable/ws`) |
| particle | `ergo_particle_link` | `particle` (`/particle/ws`)、 受信のみ |
| scene | `ergo_scene_link` | `scene_editor` (`/scene_editor/ws`) |
| log | `ergo_log_live` | `log` (`/log/ws`)、 送信のみ |

## 作業

### 入力
- `jsonm::parse` に渡される JSON テキスト
- `Client::start(host, port, path)` / `stop()` / `send_text(payload)`
- 接続先から届く WS フレーム

### 出力
- `jsonm::serialize` の JSON テキスト
- `on_open` / `on_text` コールバック (ワーカースレッド)
- マスク付きのクライアントテキストフレーム

### タスク
- JSON の解析 (深さ制限・末尾チェック付き) と直列化
- WS ハンドシェイク (HTTP Upgrade) とクライアントフレームのマスク
- 再接続ループ、 フレーム上限の強制、 `stop()` の即時性

## テスト
- `test_json_min` (`tests/common/`): プリミティブの解析、 オブジェクトと
  数値配列の round-trip、 不正入力の拒否
- `test_ws_client` (`tests/common/`): 127.0.0.1 のポート 0 で立てた
  プロセス内ループバックサーバを相手に、 ハンドシェイク / マスク付き送信
  (7・16・64 bit 長) / サーバのテキスト受信 / 101 と同じ受信に載った
  フレーム / close フレーム後の再接続 / TCP 切断後の再接続 / 1 MiB 超の
  ヘッダで切断して再接続 / 切断中の送信破棄 / サーバ不在・無応答・接続待ち
  (SYN が捨てられる) 中・相手が読まない送信中の `stop()` の即時性。 待ち合わせは期限付きのポーリングで、 固定ポートは
  使わない
