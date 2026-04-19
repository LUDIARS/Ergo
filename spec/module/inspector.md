# Inspector モジュール定義

## 概要
動作中アプリケーションの内部変数を**外部ブラウザから直接参照・書き換え**する開発者ツール基盤。
ホストアプリは型消し API で変数 (POD/関数アクセサ) を登録するだけ。Inspector は WebSocket サーバを
1スレッドで起動し、JSON プロトコルで列挙・取得・更新・変更通知を提供する。
リリースビルドでは CMake オプションで完全に切り離せる (シンボル含めゼロコスト)。

## カテゴリ
システム

## 所属ドメイン
開発者ツール / デバッグ / ライブチューニング

## 必要なデータ
- 変数登録要求 (名前, 型, 値ポインタ or getter/setter, メタデータ)
- メタデータ (最小値・最大値・ステップ・カテゴリ・単位・read-only フラグ)
- WebSocket 接続要求 (HTTP Upgrade)
- クライアント送信 RPC (op: enumerate / get / set / subscribe)
- リスニングポート番号 (デフォルト 17317)
- 同梱ブラウザ UI (`tools/inspector_web/index.html`)

## 依存
- C++17 標準ライブラリ
- Threads (std::thread)
- OS ソケット (Winsock / BSD sockets)
- (テスト) GoogleTest

## 変数
- 登録済み変数テーブル (name → Tweakable)
- 書き込みコマンド MPSC キュー (ネットスレッド → メインスレッド drain)
- 接続中クライアント一覧 (file descriptor + 部分受信バッファ + サブスクリプション集合)
- サーバ稼働フラグ
- リスニングソケット
- ブロードキャスト pending リスト (値変化したら全クライアントへ changed 通知)

## 作業

### 入力
- ホストからの登録呼び出し (`register_value` / `register_accessor` / マクロ展開)
- ブラウザからの WebSocket メッセージ (JSON)
  - `{"op":"enumerate"}` — 全変数列挙
  - `{"op":"get","name":...}` — 単一値取得
  - `{"op":"set","name":...,"value":...}` — 値更新要求
  - `{"op":"subscribe","name":...}` / `{"op":"unsubscribe",...}` — 変更通知購読
- ホストからのフレームポイント呼び出し (`apply_pending_writes`)

### 出力
- ブラウザへの WebSocket レスポンス (JSON)
  - 列挙結果 (vars 配列)
  - 単一取得結果
  - set 成否
  - changed 通知 (該当購読者へ自発配信)
- ホスト変数の更新 (登録時に渡された setter 経由)

### タスク
- 変数登録/解除 (型消しで保持, 重複名は警告 + 上書き)
- WebSocket サーバ起動・停止 (1スレッド, accept ループ + 全接続を select/poll)
- HTTP Upgrade ハンドシェイク (Sec-WebSocket-Accept = base64(sha1(key + magic)))
- WebSocket フレーム encode/decode (text/binary/ping/pong/close, masking, ≥2 byte length)
- 最小 JSON encode/decode (登録メタの平坦オブジェクトに限定)
- set 受信時に MPSC キューへ enqueue (即時実行はしない, スレッド分離)
- フレーム頭で apply_pending_writes() が drain → setter 呼び出し → 変化があれば changed broadcast
- 切断検出, ping/pong による keepalive (30s)
- ダミープラグ提供 (リンクのみ満たす no-op 実装)
- リリース分離: `ERGO_INSPECTOR_ENABLED` OFF 時は登録マクロが空展開, server 関連 cpp は除外

### プラットフォーム別タスク
- **Windows**: Winsock 初期化 (WSAStartup), `WSAPoll`, `closesocket`
- **Linux/macOS**: BSD socket, `poll(2)`, `close`, `pipe(2)` で wakeup
- **WebGL**: 対象外 (ブラウザ側で動くため自分自身を inspect する用途は想定しない)

# テスト
- 値型変数の登録 → enumerate 結果に含まれること
- アクセサ登録 → setter が apply_pending_writes() で呼ばれること
- 重複名の登録は警告ログを出すが落ちないこと
- メタデータが enumerate レスポンスに含まれること
- 不正な型の set リクエストは ok:false を返すこと
- 範囲外の set リクエストはクランプされること (meta.min/max があれば)
- ERGO_INSPECTOR_ENABLED=OFF でもビルド・リンクが通ること (ダミー使用)
- WebSocket ハンドシェイクが Sec-WebSocket-Accept を正しく返すこと
- マスクされたフレームを正しく decode すること
- 64KB 超のフレームを正しく扱えること
- ping を受けて pong を返すこと
- 1接続が異常終了しても他接続が影響を受けないこと
- changed 通知が subscribe 中のクライアントにのみ届くこと
- 同時複数クライアントが accept → enumerate → set できること
