# ergo_audio の Sonus バックエンド モジュール定義

## 概要

`ergo_audio` (ゲーム SE / BGM のファサード、[`../feature/module/audio.md`](../feature/module/audio.md)) の
バックエンドの 1 つとして、LUDIARS **Sonus** (C++20 のゲーム向けサウンドランタイム) を使う。
FMOD と同じ `ergo::audio::Engine` の契約 (引数・戻り値・失敗のしかた) の裏に Sonus を置き、
CMake の `ERGO_AUDIO_BACKEND` で FMOD / Dummy と差し替える。

- 選ぶのは明示したときだけ (`-DERGO_AUDIO_BACKEND=sonus -DERGO_SONUS_DIR=<Sonus のチェックアウト>`)。
  既定の `auto` は今までどおり FMOD を探し、無ければ Dummy。Sonus を選ばない構成では Sonus も
  CMake 3.24 も要らない。
- Sonus は取り込まない (同梱も FetchContent もしない)。`ERGO_SONUS_DIR` のソースを
  `add_subdirectory` で読み (Sonus `spec/interface/CONTRACTS.md` D-100)、`Sonus::sonus` (C API) だけを
  `PRIVATE` でリンクする。`ergo_audio` は C++17 のまま、公開ヘッダに Sonus の型は出ない。
- Sonus 固有の設定 (出力先・ライブリンク・同時再生数) と計数は、別の公開ヘッダ
  `include/ergo/audio/sonus_options.h` に Sonus の型を使わずに置く。Sonus 版でだけ定義される
  (`ERGO_AUDIO_BACKEND_SONUS` で囲んで呼ぶ)。
- ライブリンクを開けると、Ergo Studio の `sonus` プラグイン ([`../feature/tool/sonus.md`](../feature/tool/sonus.md))
  がゲームにつながり、ミキサー・プロファイラ・上書き・録画を扱える。
- `ergo_sound` (外部依存なしの自前エンジン) とは無関係で、触らない。

## カテゴリ

システム

## 所属ドメイン

オーディオ (`audio`、`spec/domains/audio.domain.json`。`src/audio/sonus/` も `src/audio/` の下なので同じドメイン)

## 必要なデータ

- WAV ファイル (`load_sound` のパス)。PCM 8 (unsigned) / 16 / 24 / 32 bit、IEEE float 32 / 64 bit、
  `WAVE_FORMAT_EXTENSIBLE` (中身がそのどれか)、1〜8 チャンネル
- `sonus::Options` (初期化の前にホストが渡す。既定: OS の出力デバイス、ライブリンクなし、同時再生 128)
- ビルド時: `ERGO_SONUS_DIR` (Sonus のソース)。CMake 3.24 以上、gcc / clang / MinGW

## 依存

- C++17 標準ライブラリ (`<thread>` / `<mutex>` / `<atomic>` / `<fstream>`)
- Sonus `Sonus::sonus` (C API `snd/sonus.h`、`PRIVATE`)。Sonus の静的ライブラリ群と OS の音声ライブラリ
  (Linux: ALSA (見つかれば)、Windows: WASAPI、macOS: Core Audio) がリンクで付いてくる
- `Threads::Threads`
- テスト: mini-gtest

## 変数

- `Engine::Impl` (`src/audio/audio_engine_sonus.cpp`)
  - `snd_system* sys`、初期化したスレッド (`owner`)
  - サウンドハンドル → デコード済みの PCM (`shared_ptr<const PcmSound>`)
  - 再生待ちの列 (`play()` が積み、`update()` が始める)
  - `PlayBuffers` (再生ごとの user buffer とその解放待ち)、`SilentOutputDevice` (`Output::Silent` のとき)
- `sonus::Options` の保管 (`src/audio/sonus/sonus_options.cpp`)
- `sonus::Stats` の写し (`update()` が書き、`stats()` が任意のスレッドから読む)

| ファイル | 責務 |
|---|---|
| `src/audio/audio_engine_sonus.cpp` | `Engine` の Sonus 版 (契約、スレッド、コールバック) |
| `src/audio/wav_pcm.{h,cpp}` | WAV → float の平面 PCM (バックエンドに依存しない) |
| `src/audio/sonus/play_buffers.{h,cpp}` | 再生 1 回 = static user buffer 1 つ。作成・再生・破棄・解放 |
| `src/audio/sonus/silent_output_device.{h,cpp}` | 音を出さない出力デバイス (snd/ext.h の関数表 + タイマースレッド) |
| `src/audio/sonus/sonus_options.cpp` | `configure()` / `options()` |
| `src/audio/sonus/result_text.{h,cpp}` | `snd_result` / 終了理由の名前 (ログ用) |
| `include/ergo/audio/sonus_options.h` | Sonus 版の設定と計数 (Sonus の型なし) |
| `cmake/ErgoSonus.cmake` | Sonus の取り込みと前提の検査 |

## 作業

### 入力

- `sonus::configure(Options)` — `initialize()` の前。初期化済み・範囲外・ライブリンクの無い Sonus で
  `live_link` を求めたときは `false` を返してログに出す (設定は変わらない)
- `Engine::initialize()` — `snd_config_init` → 出力 (`Platform` = Sonus のプラットフォームのデバイス、
  `Silent` = `SilentOutputDevice`)、`max_user_buffers = max_concurrent_plays`、
  `max_pitch_ratio = SND_MAX_PITCH_RATIO`、ライブリンク → `snd_create`。失敗は `false` + ログ
  (デバイスが無ければ `SND_ERR_NO_DEVICE` と、`Output::Silent` で動かせることを出す)
- `Engine::load_sound(path)` — WAV を読んでメモリに展開。失敗は `INVALID_SOUND` + 1 行のログ
- `Engine::play(handle, volume, pitch)` — 範囲の丸めは FMOD 版と同じ (volume 0..1、pitch は 0.01 以上)。
  再生待ちの列に積む (どのスレッドからでも)
- `Engine::update()` — 初期化したスレッドから毎フレーム
- `Engine::unload_sound` / `shutdown()`

### 出力

- 戻り値は FMOD 版と同じ (`bool` / `SoundHandle`、`INVALID_SOUND` = 0)。`backend()` = `Backend::Sonus`
  (列挙の末尾に追加)、`backend_name()` = `"Sonus"`
- `sonus::stats()`: `running` / `plays_held` / `blocks_rendered` / `master_peak` (マスターのメーター) /
  `output_peak` (`Silent` の出力の最大値) / `live_link_port` / `dropped_plays`
- ログは stderr の `[audio:sonus] ...` (FMOD 版の `[audio:fmod]` と同じ形)

### タスク

- **再生**: Sonus には「ゲームが持つ波形を音量・ピッチ付きで鳴らす」 API が無い (イベントはバンクの
  もの、user buffer には再生ごとの音量もピッチも無い)。そこで再生 1 回ごとに static user buffer を作る:
  音量を掛けた波形のコピーを `sample_rate = 元のレート × pitch` で渡し (Sonus が内部レートへ
  リサンプルする)、`snd_play_user_buffer` でマスターへ鳴らす。終わり (`on_end`) で
  `snd_destroy_user_buffer`、`snd_get_user_buffer_released` が 1 になった `update()` で波形を捨てる。
  再生中に `unload_sound` しても、その再生は自分の写しで最後まで鳴る。
- **スレッド**: `snd_create` を呼んだスレッドが Sonus の所有スレッドになる。`update()` / `shutdown()` を
  別のスレッドから呼ぶと、ログに出して何もしない (Sonus の `SND_ERR_WRONG_THREAD` を起こさない)。
  `play()` / `load_sound()` / `unload_sound()` はどのスレッドからでもよい (再生は列に積むだけ)。
- **遅れ**: Sonus は記録したコマンドを `snd_update()` で公開するので、`play()` から音が出るまでは
  FMOD と同じく次の `update()` まで。
- **終了**: `shutdown()` で `snd_destroy` (音を止め、ゲームのメモリを参照しなくなる) → 波形を捨てる。
- **配備**: Sonus は静的ライブラリで、OS の音声ライブラリはシステムのもの。`ergo_audio_stage_runtime()` は
  Dummy と同じく何もしない (`ERGO_AUDIO_BACKEND` プロパティは `sonus`)。

### Sonus に対応が無く、決められた失敗を返す機能

黙って何もしないのではなく、値で失敗を返し、ログに 1 行出す。

| 機能 (FMOD 版ではできる) | Sonus 版の結果 | ログ |
|---|---|---|
| WAV 以外 (OGG / MP3 / FLAC / FSB など) の `load_sound` | `INVALID_SOUND` | `load_sound('<path>') failed: not a RIFF WAVE file (this backend decodes WAV only; ...)` |
| 圧縮 WAV (ADPCM など)、12 bit などの WAV、9 チャンネル以上 | `INVALID_SOUND` | `... compressed WAV (format tag N) ...` / `unsupported WAV sample size` / `channel count` |
| 同時に `max_concurrent_plays` を超える再生 | その再生を捨てる (`dropped_plays` が増える) | `play dropped: no free Sonus user buffer (...)` |
| Sonus がリサンプルできる比 (`SND_MAX_PITCH_RATIO` = 32、元のレート × pitch ÷ 48 kHz) を超える pitch | その上限で鳴らす (丸め) | `pitch X exceeds Sonus' resampling limit ...; clamped` |
| 出力デバイスが無い環境の `initialize()` (`Output::Platform`) | `false` | `snd_create failed: SND_ERR_NO_DEVICE (no output device; sonus::Output::Silent runs without one)` |
| 初期化したスレッド以外からの `update()` / `shutdown()` | 何もしない | `update() must run on the thread that called initialize(); ignored` (`update` は 1 回だけ) |

FMOD 版でも無いもの (3D、ストリーム、DSP、イベント) は、どのバックエンドでも `Engine` に API が無い。

### プラットフォーム別タスク

| 環境 | 出力 (`Output::Platform`) | 備考 |
|---|---|---|
| Linux | ALSA (Sonus が `find_package(ALSA)` で見つけたとき)。無ければ Sonus の無音タイマー | 確認済み (gcc / clang、`Output::Silent`) |
| Windows (MinGW) | WASAPI | 新しいファイルの構文検査だけ (MinGW の g++) |
| Windows (MSVC cl / clang-cl) | — | Sonus がビルドできないので **構成エラー** (`cmake/ErgoSonus.cmake`) |
| macOS | Core Audio | 未確認 |

## テスト

`tests/audio/test_ergo_audio_sonus.cpp` (`ERGO_AUDIO_BACKEND=sonus` の構成でだけ登録)。外のファイル・
デバイスに頼らない: WAV はテストが一時ディレクトリに作り、出力は `Output::Silent`。

- WAV の読み取り: 8 / 16 / 24 / 32 bit PCM、float、EXTENSIBLE が値どおり。OGG・圧縮 WAV・12 bit・
  9 チャンネル・サンプル無し・途中で切れたファイル・無いファイルは理由付きの失敗
- `configure()`: 範囲外を拒否、初期化中は拒否して設定を変えない
- 無音デバイスを通した再生: 音量 0.5 で出力の最大値が半分、範囲外の音量は FMOD 版と同じ丸め、
  再生が終わると波形が解放される (`plays_held` が 0)、再生中の `unload_sound` でも最後まで鳴る
- pitch: 2.0 で長さが半分、0.5 で倍 (Sonus が描画したブロック数で測る)
- 失敗: 未初期化の `load_sound`、WAV 以外、無いファイル、同時再生の上限を超えた分が `dropped_plays`、
  知らないハンドル
- スレッド: 別スレッドの `play()` は次の `update()` で鳴り、別スレッドの `update()` / `shutdown()` は効かない
- ライブリンク (Sonus が `SND_LIVELINK=ON` のとき): 指定したポートで TCP を受け付ける

`tests/audio/test_ergo_audio.cpp` (どのバックエンドでも同じ契約のテスト) も Sonus 版で通る
(Sonus 版では最初に `Output::Silent` を設定する)。
