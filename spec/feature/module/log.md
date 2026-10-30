# Log モジュール定義

## 概要

開発用の簡易ロガー。4 レベル (**Error / Warn / Info / Debug**) を持ち、
各出力行の先頭に **現在のフレーム番号** をダンプする。フレーム番号は
`ergo_frame` の `count()` が既定ソース (ホストが `set_frame_provider` で
差し替え可能)。

出力は **シンク** を通る。 既定で入っているコンソールシンクが
Error / Warn を stderr、Info / Debug を stdout に出力する (従来どおりの
出力)。 ホストは `add_sink` で出力先を足し、 `remove_sink(&console_sink())`
でコンソールを止められる。 各行にはフレーム番号に加えて **時刻** (注入
可能な時刻ソース) とスレッド id が付き、 `ergo_profile` のタイムラインと
突き合わせられる。

任意ターゲット `ergo_log_live` の `LiveSink` は、 行を統合開発者ツール
(`tools/ergo`) の `log` プラグインへ WebSocket で流す。 GUI アプリや
Electron / Custos 経由の実行でも、 ブラウザでレベル・文字列・フレーム範囲
を絞り込みながら動作中のログを読める (プラグイン側は
[`spec/feature/tool/log.md`](../tool/log.md))。

レベルフィルタにより指定レベル以下のみ出力する。既定は `Info`
(Debug を抑制)。 フィルタはシンクより前に効く。

## カテゴリ
システム

## 所属ドメイン
デバッグ / 開発者ツール

## 必要なデータ
- カレントレベル (Error=0 / Warn=1 / Info=2 / Debug=3)
- フレーム番号プロバイダ (`std::function<uint64_t()>`)
- 時刻プロバイダ (`std::function<int64_t()>`、 µs)
- シンク一覧 (`Sink*` の配列。 追加・削除のたびに作り直すスナップショット)
- コンソール出力用 mutex

## 依存
- C++17 標準ライブラリ (`<mutex>`, `<atomic>`, `<chrono>`, `<cstdio>`,
  `<cstdarg>`, `<functional>`, `<thread>`)
- (任意) `ergo_frame` — ホストが `set_frame_provider(&ergo::frame::count)`
  でつなぐ
- (任意) `ergo_profile` — ホストが `set_time_provider(&ergo::profile::now_us)`
  でつなぐ。 `ergo_log` 自身は `ergo_profile` を import しない
- (任意) `ergo_log_live` — `LiveSink`。 `ergo_log` + `ergo_common_ws`
  (共有 WebSocket クライアント、 [`common.md`](common.md)) + `ergo_common`
  (jsonm)。 `ERGO_BUILD_LOG_LIVE` (既定 ON) で切り離せる。 `ergo_log` 本体は
  ソケットに依存しない
- (テスト) GoogleTest 互換 mini-gtest

## 変数
- `threshold` : カレントフィルタレベル (atomic)
- `frame_provider` : フレーム番号取得関数
- `time_provider` : 時刻取得関数 (空なら初回使用時からの steady_clock µs)
- `sinks` : インストール済みシンクのスナップショット (`sinks_mtx` で保護)
- `print_mtx` : コンソールシンクの出力排他 mutex

## 作業

### 入力
- `log(Level, fmt, ...)` 呼び出し (または `ERGO_LOG_*` マクロ)
- `set_level(Level)` によるレベル変更
- `set_frame_provider(fn)` によるフレームソース差し替え
- `set_time_provider(fn)` による時刻ソース差し替え
- `add_sink(Sink*)` / `remove_sink(Sink*)` によるシンクの付け外し

### 出力
- コンソールシンク: `[F<frame>][<level>] <message>` 形式で 1 行
  - 例: `[F00012345][INF] player connected`
  - レベルごとに出力先 (stderr / stdout) を切り替え
- 各シンクに `Record` を 1 つずつ渡す (下記)

### タスク
- printf スタイル `vsnprintf` で **1 回だけ**整形し、 全シンクで共有する
- レベルフィルタで早期 return (シンクには届かない)
- シンク一覧のスナップショットを取り、 ロックを持たずに各シンクを呼ぶ
- コンソールシンクは `print_mtx` 保持中に書き込み + fflush
- ダミープラグ (`log` / `add_sink` などは空) を提供

## シンク

```cpp
namespace ergo::log {
struct Record {
    Level            level;
    uint64_t         frame;   // フレームプロバイダの値
    int64_t          ts_us;   // 時刻プロバイダの値 (µs)
    uint64_t         tid;     // std::hash<std::thread::id> (ergo_profile の tid と同じ)
    std::string_view msg;     // 整形済み本文 (プレフィクスなし)。 on_record の間だけ有効
};
class Sink {
public:
    virtual ~Sink();
    virtual void on_record(const Record& r) = 0;
};
void  add_sink(Sink* sink);
void  remove_sink(Sink* sink);
Sink& console_sink();
}
```

- **既定はコンソールシンクだけ**。 何もしなければ従来と同じ出力になる。
  GUI アプリはコンソールを `remove_sink(&console_sink())` で止められ、
  `add_sink(&console_sink())` で戻せる
- シンクは追加順に呼ばれる。 同じシンクの二重追加と nullptr は無視する。
  シンクは所有しない (取り外すまで生かしておくのはホストの責任)
- `msg` はログ呼び出しが持つバッファを指すので、 残すならコピーする
- シンクが例外を投げても握りつぶし、 後続のシンクは呼ばれる

### スレッドと再入

- `on_record` は **ログを出したスレッド上で、 ergo_log のロックを持たずに**
  呼ばれる。 シンクの実装はスレッドセーフにする
- シンク一覧は `sinks_mtx` の下でスナップショット (共有ポインタ) を取る
  だけで、 呼び出し中はロックを持たない。 追加・削除は新しい一覧を作って
  差し替える
- **`remove_sink` は、 そのシンクを呼んでいる途中のスレッドが無くなるまで
  待ってから返る**。 返った直後にシンクを破棄してよい。 例外として、
  シンクの中から呼んだ `remove_sink` は取り外すだけで待たない (自分自身が
  呼び出し中なので待つとデッドロックする)
- シンクの中 (同じスレッド) で出したログは **捨てる** (`thread_local` の
  フラグ)。 シンクが自分に再帰することはない。 シンクの中から
  `add_sink` / `remove_sink` を呼ぶのは構わない
- `set_level` は atomic。 `set_frame_provider` / `set_time_provider` は
  他スレッドがログを出す前の起動時に呼ぶ (差し替えは atomic ではない)

## 時刻ソース

`set_time_provider(fn)` で各行の `ts_us` を決める。 空 (既定) なら
ロガーを最初に使った時点からの `steady_clock` µs。 `ergo_profile` の
タイムラインに合わせるには `&ergo::profile::now_us` をつなぐ (フレーム
番号の `set_frame_provider` と同じ考え方)。 プロバイダが例外を投げたら 0。

## LiveSink — `ergo_log_live`

`ergo::log::LiveSink` (`include/ergo/log/live_sink.h`) は
`ws://host:5170/log/ws` に接続し、 行を `log` プラグインへ送るシンク。

```cpp
#include "ergo/frame/frame.h"
#include "ergo/log/live_sink.h"
#include "ergo/profile/profile.h"

ergo::log::set_frame_provider(&ergo::frame::count);
ergo::log::set_time_provider(&ergo::profile::now_us);

static ergo::log::LiveSink live;
ergo::log::add_sink(&live);              // ここからの行はキューに入る
live.start("127.0.0.1", 5170, "mygame"); // 非同期。 切れても自動で再接続
// ergo::log::remove_sink(&ergo::log::console_sink());  // GUI アプリなら
...
ERGO_LOG_INFO("hi");                     // /log/ にフレーム番号付きで出る
```

- `start(host = "127.0.0.1", port = 5170, app = "app")` — 接続を始める。
  既に開始済み、 または host / port / app が空なら false。 シンクの
  インストールはしない (`add_sink` は別に呼ぶ)
- `stop()` — ワーカーとソケットを止める。 未送信の行はキューに残り、
  次の `start()` で送られる。 何度呼んでもよい
- `is_connected()` — WebSocket セッションが開いている間 true
- デストラクタは `remove_sink(this)` してから `stop()` する。 ホストが
  外し忘れても、 破棄後に呼ばれることはない
- `on_record` はレコードを **上限付きキュー** (既定 4096 件、
  `LiveSink(capacity)` で変更可) にコピーするだけで、 ソケットを待たない。
  満杯なら **古いものから捨てて数え**、 次のフレームの `dropped` で
  ツールに報告する。 8 KiB を超える本文は UTF-8 の境界で切って `…` を付ける
- ワーカースレッドが約 50 ms ごとに、 接続中ならキューを最大 512 件ずつ
  `lines` フレームにして送る。 接続が開くたびに (再接続を含む) まず
  `{op:"hello", role:"engine", app}` を送る。 ツールが落ちている間の行は
  キューに溜まり、 つながったら送られる (上限を超えた分は捨てて数える)
- **LiveSink 自身は ergo_log でログを出さない**。 失敗は `start()` の戻り値と
  `is_connected()` で分かる
- `take_frame(max_lines)` — キューの先頭から最大 `max_lines` 件と未報告の
  drop 数を 1 つの `lines` フレームにして取り出す (何も無ければ空文字列)。
  ワーカーが使うもので、 テストや自前の転送路を持つホストも使える
- `encode_hello_frame(app)` / `encode_lines_frame(app, lines, dropped)` —
  フレームを作る純関数 (jsonm)。 ワイヤ形式は
  [`spec/feature/tool/log.md`](../tool/log.md)
- `pending()` / `dropped()` — キュー内の件数 / 生成以降に捨てた累計

既知の制限:

- 接続が切れる瞬間に渡したフレームは失われる (`dropped` には数えない)
- `stop()` は `ergo_common_ws` の送信キューを待たないので、 直前に渡した
  フレームが届かないことがある
- 数値は JSON の double。 `tid` (64 bit ハッシュ) は 2^53 を超えると下位
  ビットが丸まるが、 スレッドを見分ける用途には足りる (`ergo_profile` の
  トレースを JS で読んだときと同じ丸めになる)

## テスト
- 各レベルで出力されること (stdout キャプチャ) — `test_log`
- フィルタが効くこと (Debug を隠す設定で Debug 行が出ない)
- `set_frame_provider` で注入したフレーム番号がプレフィクスに出ること
- マクロが展開されること
- マルチスレッド同時書き込みで行が混ざらないこと (ベストエフォート)
- `test_log_sink`: キャプチャシンクにレベル・フレーム・本文・tid が届く /
  長い本文がそのまま届く / レベルフィルタがシンクより前に効く / コンソール
  シンクを外すと stdout が止まり、 戻すと従来形式で出る / 二重追加は無視 /
  シンク内のログは捨てられる / シンクが自分を外してもデッドロックしない /
  時刻プロバイダ (注入・例外・既定の単調増加) / `remove_sink` が呼び出し
  中のシンクを待つ / 4 スレッドの追加・削除とログの同時実行で、 外した
  シンクが呼ばれない
- `test_live_sink` (ソケットなし): `hello` / `lines` フレームの形と
  エスケープ / キューの上限と drop 数 (古い順に捨てる、 報告は 1 回) /
  既定容量とバッチ上限 / 長い本文の切り詰め / ergo_log 経由で届くことと、
  デストラクタでの自動取り外し / `start()` の引数チェック
