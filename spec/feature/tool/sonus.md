# サウンド (`sonus` プラグイン) {#SPEC-ergo-sonus}

依頼 (2026-11-19): Ergo に LUDIARS Sonus (C++20 のゲーム向けサウンドライブラリ) を組み込む。エンジン側は
`ergo_audio` の Sonus バックエンド ([`../../module/audio_sonus.md`](../../module/audio_sonus.md))、ツール側は
ネイティブ版 Ergo Studio の組み込みプラグインとして、動いているゲームの Sonus にライブリンクでつなぎ、
ミキサー・プロファイラ・上書き・録画を扱う。

## 置き場所

ネイティブ版 Ergo Studio (`tools/studio`) の組み込みプラグイン。**Sonus を取り込んだビルドでだけ**作られ、
登録される (`ERGO_AUDIO_BACKEND=sonus` か `ERGO_WITH_SONUS=ON`、どちらも `ERGO_SONUS_DIR` が要る。
それ以外の構成では `sonus` プラグインは存在しない)。`IPluginHttp` (`/sonus/api/*`)、`IPluginHealth`、
`IPluginView` (タブ「サウンド」) を持ち、WS の経路は持たない (画面とスクリプトは state をポーリングする。
ゲームからの入力はライブリンクで届くので、Studio のサーバに受け口が要らない)。

Sonus のツールの核 (`Sonus::tool_core`、D-101) は C++20 なので、Sonus に触る 1 ファイルを別のターゲットに
分け、Studio 本体とプラグインの残りは C++17 のまま保つ。

| 層 | ファイル | ターゲット | 責務 |
|---|---|---|---|
| 設定 | `plugins/sonus/sonus_config` | `ergo_studio_plugins` (常に) | `ERGO_SONUS_*` の読み取り (純関数) |
| 表示用の型 | `plugins/sonus/sonus_shown` | `ergo_studio_sonus_link` | Sonus のヘッダを含まない C++17 の構造体 |
| ライブリンク | `plugins/sonus/sonus_link` | `ergo_studio_sonus_link` (**C++20**) | `snd::tool::Session` (接続・ミキサー・上書き・プロファイラ・録画) を C++17 の IF の裏に置く |
| プラグイン | `plugins/sonus/sonus_plugin` | `ergo_studio_sonus` | 定期更新のスレッド、排他、ログ、health、録画の保存先 |
| 経路 | `plugins/sonus/sonus_routes` | `ergo_studio_sonus` | `/sonus/api/*` (変更系の防御付き) |
| JSON | `plugins/sonus/sonus_shown_json` | `ergo_studio_sonus` | タブと同じ内容の JSON |
| 録画のファイル名 | `plugins/sonus/recording_path` | `ergo_studio_sonus` | `sonus-<UTC 日時>-<n>.snrc` |
| 画面 | `views/sonus_view` | `ergo_studio_sonus_view` | タブ「サウンド」(ergo_ui_kit) |
| 組み立て | `builtin_plugins.cpp` | `ergo_studio` | `ERGO_STUDIO_HAS_SONUS` のときだけ factory を足す |

`Sonus::tool_core` は `ergo_studio_sonus_link` に `PRIVATE` でリンクするので、`cxx_std_20` はそこで止まる。

## 接続先 {#SPEC-ergo-sonus-target}

| 変数 | 既定 | 内容 |
|---|---|---|
| `ERGO_SONUS_HOST` | `127.0.0.1` | ゲームのライブリンクのアドレス |
| `ERGO_SONUS_PORT` | `27460` | ゲームのライブリンクのポート (1..65535)。既定値は Sonus 自身の既定 (`livelink::kDefaultPort`。`sonus_link.cpp` がコンパイル時に一致を検査する) |
| `ERGO_SONUS_PROJECT` | 無し | Sonus のプロジェクト (バス名・ミキサーの元)。無ければミキサーにバスが出ない (プロファイラは動く) |
| `ERGO_SONUS_BUILD_DIR` | 無し = 読むだけ | プロジェクトをそこへビルドして build hash を得る。読むだけのときはゲームのバンクと hash が合わず、Sonus の「差分を送る」で Studio の値をゲームへ送る |
| `ERGO_SONUS_RECORDING_DIR` | 無し = 保存しない | 録画の保存先 |
| `ERGO_SONUS_TICK_MS` | 50 | セッションを回す間隔 (10..1000 ms) |

- 接続先は設定からだけ取る。経路・画面から接続先を変えることはできない。解決できない・つながらないときは
  Sonus の自動再接続 (同じ接続先) に任せ、別のポートやホストは試さない。
- ゲーム側は `ergo_audio` の Sonus 版なら `sonus::Options::live_link = true` (ポートは `live_link_port`、
  0 = Sonus の既定)、Sonus を直接使うゲームなら `snd_config.livelink`。
- 版が合わない (`HelloReply` の build hash の違い) ときは `MismatchPolicy::SendDiffs`: つながったうえで
  Studio (プロジェクト) の値を送る。プロトコルの版違いは「拒否された」と出し、再接続しない (Sonus の動作)。

## 動作 {#SPEC-ergo-sonus-behavior}

- **スレッド**: `snd::tool::Session` は 1 度に 1 スレッドから。プラグインの 1 本の定期更新スレッドが
  `tick_ms` ごとに (接続中だけ) `Tick()` して表示用の状態を作り直す。画面と経路の操作は同じ mutex の中で
  セッションを直接触り、終わったら表示用の状態を作り直す (操作の結果はすぐ state に出る)。ネットワークは
  Sonus のライブリンクのクライアントのスレッドが行う。
- **ミキサー**: バスごとのフェーダー (dB、-144 = 切)、ミュート、ソロ (Sonus の規則: ソロのバスの上下以外は
  「ソロで消音」)。どの操作も 1 回で 1 つの元に戻す単位 (`MixerModel` の履歴)。元に戻す / やり直す。
  値はセルの同期 (`CellSync`) がゲームに送り、Ack で確かめる。送信中 / 拒否 / 応答なしの数を出す。
- **上書き**: バンク (プロジェクト) の値と違う値の一覧 (`MixerModel::Overrides()`)。1 つ戻す (キーで指定)、
  全部戻す (1 つの元に戻す単位)。
- **プロファイラ**: 処理時間 (描画・ロジック・負荷・予算超過・LOD・遅れ・アンダーラン)、メーター (バスごと、
  ピークホールド、クリップ、500 ms 届かなければ「更新なし」)、ボイス (実 / 仮想、バス、状態、ソース、ゲイン)、
  メモリ (タグごとの使用量 / 予算)、ログ (直近 64 件)、ストリームの停滞、LOD の変化。
- **録画**: 開始 → 録画中 → 停止 → 録画済み → 保存。保存は `ERGO_SONUS_RECORDING_DIR` の下にだけ
  (`recording_path`)。設定が無ければ保存は無効と出す。ファイルは Sonus の録画ファイル (`SNRC`)。
- **切断**: 切ってもゲームの上書きは残る (Sonus の `revertOnDisconnect = false`)。戻すなら切る前に「全部戻す」。
- **ログ**: 接続状態の変化、セッションのメッセージ、操作の失敗、録画の保存を `[sonus] ...` で 1 行ずつ。

health (`/api/health` の `sonus`): `{ok, state, target, project}`。`ok` はプロジェクトにエラーが無いこと
(接続していなくても `true`)。

## Studio のサーバの経路 {#SPEC-ergo-sonus-http}

| 経路 | 本文 | 応答 |
|---|---|---|
| `GET /sonus/api/state` | — | `200` タブと同じ内容 (`state` / `target` / `project` / `mixer` / `overrides` / `profiler` / `recording` / `messages`) |
| `POST /sonus/api/connect` | `{}` | `200 {ok}` / `409 already_connected` / `502 connect_failed` |
| `POST /sonus/api/disconnect` | `{}` | `200 {ok}` |
| `POST /sonus/api/mixer/bus` | `{"bus":"<key>", "fader_db":<数> \| "mute":<bool> \| "solo":<bool>}` (どれか 1 つ) | `200 {ok, changed}` / `400 invalid_request` / `404 not_found` (そのキーのバスが無い) |
| `POST /sonus/api/mixer/undo` / `redo` | `{}` | `200 {ok, changed}` |
| `POST /sonus/api/overrides/revert` | `{"key":"<上書きのキー>"}` | `200 {ok, changed}` / `404 not_found` |
| `POST /sonus/api/overrides/revert-all` | `{}` | `200 {ok, changed}` |
| `POST /sonus/api/recording/start` / `stop` | `{}` | `200 {ok}` / `409` (ゲームにつながっていない・録画中 / 止める録画が無い) |
| `POST /sonus/api/recording/save` | `{}` | `200 {ok, path}` / `409 not_saved` (録画が済んでいない、ファイルの失敗) / `503 saving_off` (保存先が未設定) |

- 失敗は `{error, message}`。方法違いは `405`、他の経路は `404`。
- GET 以外は `studio` / `lesson` プラグインと同じ変更系の防御 (`plugins/common/mutation_guard`、
  [ST-INV-03](studio.md#SPEC-ergo-studio-guard): loopback の Host、Origin を付けるなら同じ host:port、本文が JSON) を通る。
  別サイトのページから localhost へ POST して、ゲームの音を消させる・録画を書かせることはできない。

## 画面 (タブ「サウンド」) {#SPEC-ergo-sonus-view}

- 上: 「接続」/「切断」、状態 (未接続 / 接続を試しています / 接続の確認中 / 接続済み / 拒否)、接続先、ゲームの
  サンプルレートとブロック、RTT、版の違い、プロジェクト、直近のメッセージ。
- ページ: ミキサー / プロファイラ / 上書き (件数) / 録画。
- ネイティブ版 Studio の UI にはまだスライダーも文字入力も無いので、フェーダーはボタン (-6 / -1 / +1 / +6 dB、
  0 dB、切) で動かし、メーターは棒で描く (左端 -60 dB、-6 dB を越えると黄、クリップで赤、更新なしで灰)。

## 不変条件

### SN-INV-01 接続先は設定だけ {#SPEC-ergo-sonus-inv-target}

ゲームのアドレスとポートは `ERGO_SONUS_HOST` / `ERGO_SONUS_PORT` からだけ取る。コードにあるポートは既定値の
`27460` (Sonus の既定) だけで、他を試さない ([ST-INV-01](studio.md#SPEC-ergo-studio-endpoints) と同じ考え方)。

### SN-INV-02 書き込み先は設定だけ {#SPEC-ergo-sonus-inv-write}

ディスクに書くのは録画の保存 (`ERGO_SONUS_RECORDING_DIR` の下、名前は Studio が決める) と、
`ERGO_SONUS_BUILD_DIR` へのプロジェクトのビルドだけ。経路の本文からパスを受け取らない。

### SN-INV-03 Studio 本体は C++17 {#SPEC-ergo-sonus-inv-cxx}

Sonus のヘッダを含むのは `sonus_link.cpp` だけ。そこを含むターゲットだけが C++20 になる。

### SN-INV-04 Sonus の無いビルドは変わらない {#SPEC-ergo-sonus-inv-optional}

Sonus を取り込まない構成では、`sonus` プラグインのターゲットもテストも作らず、`builtin_plugins()` にも出ない。
設定 (`ERGO_SONUS_*`) は読むが使わない (`--help` には「builds with Sonus」と出る)。

## 検証

- `tests/studio/test_studio_sonus.cpp` (Sonus を取り込んだ構成でだけ登録): 同じプロセスで Sonus のゲーム
  (C API、Sonus 内蔵の手で進める無音デバイス `SND_OUTPUT_MANUAL`、ファイルシステムの根 `file_system_root` を
  一時ディレクトリにしてバンクを名前で読む、ライブリンクを空いているループバックのポートで) を動かし、プラグインをつなぐ。
  ゲームの 1 フレームは `snd_update` + `snd_advance_output` 1 周期で、デバイスに渡った波形をテストが受け取る。
  - 接続 (経路)、二重接続は 409、版が合う、サンプルレート
  - プロファイラ: マスターのメーターがゲームの出力と同じ、ボイス (2 本以上、`Sfx` の名前)、処理時間、メモリ、
    メーターの名前、接続中に鳴らしたイベントがログに出る
  - ミュート (経路): **ゲームの出力 (メーターとデバイスに渡った波形) が半分になり**、Studio のメーターが追う。
    上書き 1 件 (消音)、元に戻す 1 件
  - 全部戻す: 上書きが空になり、ゲームの出力が元に戻る
  - フェーダー -6 dB、同じ値は「変化なし」、元に戻す・やり直す、+6 dB で 0 dB
  - ソロ: 他のバスが「ソロで消音」、その上書きをキーで 1 つ戻す、無いキーは 404
  - 録画: 済む前の保存は 409、開始 → 録画中 → 記録が届く → 停止 → 保存 (保存先の下、ヘッダより大きい)
  - state の JSON、health、ログ、切断後もゲームはプロジェクトの音量のまま
  - プロジェクト無し・ゲーム無し: 何も無い操作は `NotFound` / `Unchanged`、録画できない、保存が無効 (503)、
    経路の 404 / 405 / 403 (別ホスト・別サイトの Origin・JSON でない本文) / 400
  - エラーのあるプロジェクト: health の `ok` が false、ログに出る
- `tests/studio/test_studio_sonus_view.cpp`: ページの切り替えと文言、ゲーム無しでの「録画を始められません」、
  生きたゲームでの接続・ミュート・元に戻す・-6 dB・上書きのページ・全部戻す (ボタンを押して確かめる)。
- `tests/studio/test_studio_settings.cpp`: `ERGO_SONUS_*` の既定・値・壊れた値のエラー、`--help` の一覧。
- Sonus 側の課題の解消 (Sonus 契約 1.2 の D-102 / D-103): 最初の組み込みでは、テストのゲームが自前の null デバイスの
  関数表を書き、バンクを作業ディレクトリの下に置いて相対パスで読ませていた (既定のファイルシステムが作業ディレクトリ
  からの相対しか読めず、Windows では何も開けなかった)。Sonus が内蔵の手で進めるデバイスとファイルシステムの根を
  入れたので、どちらの回避も外した。
- 確認していないこと (2026-11-19 時点): Windows / macOS でのビルドと動作 (MinGW は新しいファイルの構文検査だけ)、
  実際のサウンドデバイスでの再生、窓のある Studio での表示 (Pictor + Vulkan + GLFW が要る)。
