# Profile モジュール定義

## 概要

`ergo_profile` は **パフォーマンス確認用のタイムライン**を生成するモジュール。
アプリ実行中の「速度 / メモリ / プロセス・スレッド」 を、 コードに差し込んだ
**マーカー**で計測し、 タイムライン形式で可視化できる trace を出力する。

設計の中心は **AOP 的なマーカー注入** — プロファイリングという横断的関心事
(cross-cutting concern) を、 本来のロジックを汚さずスコープ境界に「差し込む」。
C++ に言語機能としての AOP は無いため、 RAII スコープガード + マクロを
アスペクトの注入点とし、 任意 callable を包む `aspect()` を advice 相当とする。

ホストアプリは:

1. 計測したい箇所に `ERGO_PROFILE_SCOPE("name")` 等のマーカーを差し込む
2. 実行する → コレクタが marker event をスレッド別に蓄積
3. `ergo::profile::dump("trace.json")` で **Chrome Trace Event 形式**の
   タイムラインを書き出す
4. `tools/ergo` の `profile` プラグイン (または `chrome://tracing` / Perfetto)
   でタイムラインを閲覧する

`ERGO_BUILD_PROFILE=OFF` 時はマクロが空に展開され **ゼロコスト** (リリース
ビルドで計測コードを完全に消せる)。

## カテゴリ
システム

## 所属ドメイン
デバッグ / 計測 / 開発者ツール

## 必要なデータ
- marker event 列 — `{ name, phase, ts(us), dur(us), pid, tid, category, value }`
- スレッドごとの event バッファ (thread-local、 記録時ロックフリー)
- スレッド名テーブル (tid → 表示名)
- セッションの基準時刻 (`steady_clock` の epoch)
- プロセス常駐メモリ量 (RSS) — `ERGO_PROFILE_MEM` 採取時にプラットフォーム
  API で取得

## 依存
- C++17 標準ライブラリ (`<chrono>`, `<thread>`, `<atomic>`, `<mutex>`,
  `<vector>`, `<string>`, `<fstream>`)
- メモリ RSS 取得のプラットフォーム shim (Windows: `GetProcessMemoryInfo` /
  POSIX: `/proc/self/statm`) — モジュール内に閉じる
- 上位モジュールには依存しない (下層モジュール。 `ergo_log` 等から使われる側)
- (テスト) GoogleTest 互換 mini-gtest

## マーカー API

すべてマクロ。 `ERGO_BUILD_PROFILE=OFF` で空展開 (ゼロコスト)。

| マクロ | 種別 | 用途 |
|---|---|---|
| `ERGO_PROFILE_SCOPE(name)` | スコープ計測 (RAII) | **速度** — スコープの enter/exit と所要時間 |
| `ERGO_PROFILE_FUNC()`      | スコープ計測 | `ERGO_PROFILE_SCOPE(__func__)` の糖衣 |
| `ERGO_PROFILE_MARK(name)`  | 瞬間マーカー | ある時点のイベント (フレーム境界等) |
| `ERGO_PROFILE_COUNTER(name, v)` | カウンタ | 任意の数値系列 (draw call 数 等) |
| `ERGO_PROFILE_MEM(name)`   | カウンタ | **メモリ** — プロセス RSS を採取しカウンタ化 |
| `ERGO_PROFILE_THREAD(name)`| メタ | 現スレッドにタイムライン表示名を付ける |

非マクロ API (`namespace ergo::profile`):

- `aspect(Callable&& fn, const char* name) -> 戻り値` — 任意の callable を
  スコープ計測で包んで実行する (AOP の advice 相当)。 関数単位の差し込みに使う
- `set_enabled(bool)` / `is_enabled()` — 実行時 on/off (コンパイル時 ON 前提)
- `begin_session()` / `clear()` — 計測区間の制御 (epoch リセット + イベント破棄)
- `set_sink(MarkerSink*)` / `default_sink()` — マーカー出力先の差し替え (下記)
- `export_chrome_trace() -> std::string` — 蓄積イベントを JSON 文字列に
- `dump(const std::string& path)` — `export_chrome_trace()` をファイルに書く

各 event は記録時に `pid` (プロセス) と `tid` (スレッド) を自動付与する。

## マーカー sink — アプリ側拡張点

マーカーの出力先は抽象インタフェース **`MarkerSink`** で差し替えできる。
これがアプリ側に共有する拡張インタフェース — ホストアプリは独自の
`MarkerSink` を実装し、 マーカーを自前のテレメトリへ流す / ライブ送信する /
フィルタする、 等が可能。

```cpp
class MarkerSink {
public:
    virtual ~MarkerSink() = default;
    virtual void on_event(const Event& e) = 0;          // 全マーカーを受ける
    virtual void on_thread_name(uint64_t tid, const char* name) {}
};
```

- `record_*` (= マクロの実体) は `Event` を組み立て、 現在の sink の
  `on_event` に渡すだけ。 sink が出力先を決める
- 既定 sink = 内蔵コレクタ (thread-local バッファ + Chrome Trace export)
- `set_sink(MarkerSink*)` で差し替え、 `nullptr` で既定に復帰。
  `default_sink()` は内蔵コレクタを返す (復帰用 / カスタム sink からの委譲用)
- `on_event` は任意の記録スレッドから呼ばれる → **実装はスレッドセーフ必須**
- カスタム sink を差した間、 内蔵コレクタは空 = `export_chrome_trace()` も空。
  カスタム sink 利用時はアプリが自前で出力する

## trace 形式 — Chrome Trace Event

出力は **Chrome Trace Event 形式** (`{ "traceEvents": [ ... ] }`) を採用する。
業界標準で `chrome://tracing` / Perfetto でそのまま開け、 かつ `tools/ergo`
プラグインでも描ける。 独自形式は作らない。

| マーカー種別 | `ph` | 主フィールド |
|---|---|---|
| スコープ計測 | `X` (complete) | `name, ts, dur, pid, tid, cat` |
| 瞬間マーカー | `i` (instant)  | `name, ts, pid, tid, s:"t"` |
| カウンタ / メモリ | `C` (counter) | `name, ts, pid, tid, args:{<name>:value}` |
| スレッド名 | `M` (metadata) | `name:"thread_name", pid, tid, args:{name}` |

`ts` / `dur` はマイクロ秒。 ネスト (スコープ入れ子) は同一 tid 上の `X` event
の時間包含関係からビューア側が再構成する (フレームグラフ表示)。

## 変数
- `enabled_` : 実行時の計測有効フラグ (`std::atomic<bool>`)
- `session_epoch_` : セッション基準時刻 (`steady_clock::time_point`)
- `thread_buffers_` : スレッド別 event バッファの登録簿 (mutex 保護、
  各バッファ自体は thread-local で記録時は無ロック)
- `thread_names_` : tid → 表示名
- `pid_` : プロセス id (起動時に 1 回取得)

## 作業

### 入力
- ホストコードに差し込まれたマーカーマクロの実行
- `set_enabled` / `begin_session` / `end_session` / `clear` 呼び出し
- `ERGO_PROFILE_THREAD` によるスレッド名登録

### 出力
- `export_chrome_trace()` : Chrome Trace 形式 JSON 文字列
- `dump(path)` : 上記をファイルへ
- (将来) `ergo_bind` 経由のライブ送信 — タイムラインのリアルタイム表示用

### タスク
- マーカー記録: `steady_clock::now()` を取り、 thread-local バッファへ
  `Event` を push (POD・flat・事前 reserve。 [[feedback_pictor_dod_layout]] の
  方針に従い hot path で per-event alloc / pointer-chase を避ける)
- スコープ計測: RAII オブジェクトが構築時に開始時刻、 破棄時に終了時刻 +
  duration を確定し complete event を 1 件積む
- メモリ採取: `ERGO_PROFILE_MEM` でプラットフォーム API から RSS を取得し
  カウンタ event 化
- export: 全スレッドバッファをマージ → `ts` 昇順は要求しない (ビューアが
  ソート) → Chrome Trace JSON に直列化
- スレッド安全: 記録は thread-local で無ロック。 バッファ登録簿と export
  のみ mutex
- ゼロコスト: `ERGO_BUILD_PROFILE=OFF` で全マクロが `((void)0)` に展開
- ダミープラグ (no-op) をリンクのみ用に提供

## タイムライン UI (`tools/ergo` プラグイン `profile`)

C++ モジュールが出す trace を可視化する Web プラグイン。 既存の
`particle` / `rive` / `variable` と同方式で `tools/ergo/src/plugins/profile/`
に追加する (`spec/tool/ergo.md` 準拠)。

- trace JSON を読み込み (ファイルドロップ / 取得)
- **プロセス → スレッド**でレーン分割
- スコープ計測は時間包含からネストを再構成し**フレームグラフ**状に描画
- 瞬間マーカーは縦のティック
- カウンタ / メモリは折れ線トラック
- スコープ選択で duration・呼び出し回数・self time を集計表示

C++ 側 trace 形式とプラグインは同一 PR で更新する (横断変更の原則)。

## テスト
- 初期状態 — セッション未開始でイベント 0 件
- `ERGO_PROFILE_SCOPE` が complete event を 1 件積み、 `dur > 0`
- `ERGO_PROFILE_MARK` が instant event を積む
- `ERGO_PROFILE_COUNTER` / `ERGO_PROFILE_MEM` が counter event を積む
- 別スレッドで記録した event が export 時に tid 付きで現れる
- `export_chrome_trace()` が妥当な JSON (`traceEvents` 配列) を返す
- `clear()` でイベントが空になる
- `set_enabled(false)` で記録が止まる
- `ERGO_BUILD_PROFILE=OFF` 相当 (マクロ空展開) でビルドが通り計測 0 件
- `aspect()` が callable の戻り値を素通しし、 かつ計測する
- カスタム `MarkerSink` を `set_sink` で差すと `record_*` がそちらへ流れ、
  `set_sink(nullptr)` 復帰で内蔵コレクタへ戻る
