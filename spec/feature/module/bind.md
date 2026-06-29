# Bind モジュール定義

## 概要
任意のホスト変数を **`BIND_VAR()` / `bind()`** で外部の unified ergo tool (`tools/ergo/`, `variable` plugin) (Web ツール)
へエクスポートし、ブラウザ UI からのライブ編集を受け取る Ergo モジュール。

ホスト側はアウトバウンドでエディタサーバ (`ws://host:5170/variable/ws`) へ接続するため、
Win32 サーバ実装に依存しない。

> **歴史的経緯**: 旧 `ergo_inspector` モジュール (in-process WebSocket サーバ + Windows
> dummy フォールバック) は本モジュールに完全吸収され、2026-04-21 に廃止された。
> live tuning の唯一の窓口は `ergo_bind`。inspector が持っていた機能 (型消し変数登録 /
> メタデータ / 値変化検出 broadcast) は `bind` 側で全 OS 同等に動く。

## カテゴリ
システム

## 所属ドメイン
開発者ツール / デバッグ / ライブチューニング

## 必要なデータ
- 変数登録要求 (名前, 型, 値ポインタ or getter/setter, メタデータ)
- メタデータ (min/max/step/category/unit/read_only)
- 接続先 (host, port, path)
- アプリ識別子 (unified ergo tool (`tools/ergo/`, `variable` plugin) が複数 engine を区別するため)
- unified ergo tool (`tools/ergo/`, `variable` plugin) 側 wire protocol (engine ↔ server, ui ↔ server)

## 依存
- C++17 標準ライブラリ
- Threads (std::thread)
- OS ソケット (Winsock / BSD sockets) ※ アウトバウンドのみ
- (テスト) GoogleTest 互換 mini-gtest

## 変数
- 登録済み変数テーブル (name → 型消し getter/setter + meta + last_seen)
- 書き込みコマンド MPSC キュー (ネットスレッド → メインスレッドで drain)
- WS クライアント (再接続戦略付き)
- 接続フラグ
- アプリ識別子 (デフォルト exe 名 or "anonymous")

## 作業

### 入力
- ホストからの `bind(name, ptr, meta)` 呼び出し / `BIND_VAR(name, lvalue, meta)` マクロ
- ホストからの `unbind(handle)` 呼び出し
- ホストからの `apply_pending_writes()` 呼び出し
- unified ergo tool (`tools/ergo/`, `variable` plugin) からの `set` メッセージ

### 出力
- unified ergo tool (`tools/ergo/`, `variable` plugin) への `bind` (登録時), `value` (変化時), `unbind` (解除時) メッセージ
- ホスト変数の更新 (登録時に渡された setter 経由)

### タスク
- WS クライアント接続 (失敗時は指数バックオフではなく一定間隔で再試行)
- 接続成功時に `hello` を送り、登録済み全変数を `bind` で再送 (再接続シナリオ)
- 登録/解除のスレッド安全管理
- 値変化検出 (毎フレーム getter を呼んで last_seen と比較; 違えば `value` 送信)
- `set` 受信 → MPSC キューへ enqueue → apply_pending_writes() で setter 呼び出し
- ダミープラグ (リンクのみ満たす no-op)
- リリース分離 (`ERGO_BIND_ENABLED` 未定義時は BIND_VAR が空展開)

### プラットフォーム別タスク
- **Windows / Linux / macOS**: 全機能利用可能
- **WebGL**: 対象外

# テスト
- 値型変数の bind → server 側 registry に登録されること
- アクセサ bind → setter が apply_pending_writes() で呼ばれること
- 重複名の登録は警告 + 上書きで処理されること
- メタデータが登録メッセージに含まれること
- 範囲外の set 値はクランプされること (meta.min/max があれば)
- 型不一致の set は無視されること
- `apply_pending_writes` をフレーム頭で呼べば反映が次フレームで visible
- 切断中の bind 呼び出しは登録テーブルにのみ残り、再接続時に再送されること
- `ERGO_BIND_ENABLED` 未定義時にビルド・リンクが通ること (ダミー使用)
- 値変化検出: 外部スレッドから値を変えると next frame で `value` が送信されること
- 同じアプリで 100+ 変数を bind しても遅延なく動作
