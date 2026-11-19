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
  `WAVE_FORMAT_EXTENSIBLE` (中身がそのどれか)、1〜8 チャンネル。ファイルは ergo_audio が自分で読む
  (Sonus のファイルシステムは使わない。バンクを読まないので Sonus の `file_system_root` も設定しない)
- `sonus::Options` (初期化の前にホストが渡す。既定: OS の出力デバイス、ライブリンクなし、読み込める音 256)
- ビルド時: `ERGO_SONUS_DIR` (Sonus のソース、契約 1.2 の D-100〜D-104 を含むもの)。CMake 3.24 以上、gcc / clang / MinGW

## 依存

- C++17 標準ライブラリ (`<thread>` / `<mutex>` / `<fstream>`)
- Sonus `Sonus::sonus` (C API `snd/sonus.h`、`PRIVATE`)。Sonus の静的ライブラリ群と OS の音声ライブラリ
  (Linux: ALSA (見つかれば)、Windows: WASAPI、macOS: Core Audio) がリンクで付いてくる
- `Threads::Threads`
- テスト: mini-gtest

## 変数

- `Engine::Impl` (`src/audio/audio_engine_sonus.cpp`)
  - `snd_system* sys`、初期化したスレッド (`owner`)、出力の種類
  - 読み込んだ音のハンドルの集合、別スレッドで読んだ音 (次の `update()` で Sonus のバッファにする)、
    別スレッドで外した音、再生待ちの列 (`play()` が積み、`update()` が始める)
  - `SoundBuffers` (音 1 つ = static user buffer 1 つ、鳴っている再生、解放待ちのバッファ)
- `sonus::Options` の保管 (`src/audio/sonus/sonus_options.cpp`)
- `sonus::Stats` の写しと、`Output::Manual` の進め方 (`update()` / `advance_output()` が書き、`stats()` が任意のスレッドから読む)

| ファイル | 責務 |
|---|---|
| `src/audio/audio_engine_sonus.cpp` | `Engine` の Sonus 版 (契約、スレッド、コールバック)、`stats()` / `advance_output()` |
| `src/audio/wav_pcm.{h,cpp}` | WAV → float の平面 PCM (バックエンドに依存しない) |
| `src/audio/sonus/sound_buffers.{h,cpp}` | 音 1 つ = Sonus の static user buffer 1 つ。作成・再生 (`snd_play_user_buffer_ex`)・破棄・解放 |
| `src/audio/sonus/sonus_options.cpp` | `configure()` / `options()` |
| `src/audio/sonus/result_text.{h,cpp}` | `snd_result` / 終了理由の名前 (ログ用) |
| `include/ergo/audio/sonus_options.h` | Sonus 版の設定・計数・手で進める口 (Sonus の型なし) |
| `cmake/ErgoSonus.cmake` | Sonus の取り込みと前提の検査 |

## 作業

### 入力

- `sonus::configure(Options)` — `initialize()` の前。初期化済み・範囲外・知らない出力・ライブリンクの無い
  Sonus で `live_link` を求めたときは `false` を返してログに出す (設定は変わらない)
  - `output`: `Platform` (OS のデバイス) / `Silent` (Sonus 内蔵の無音、実時間で自分のスレッドが進む) /
    `Manual` (Sonus 内蔵の無音、`advance_output()` でだけ進む)。`snd_config.output_kind` の
    `SND_OUTPUT_PLATFORM` / `SILENT` / `MANUAL` に対応 (Sonus D-102)
  - `max_loaded_sounds` (1..4096): 同時に読み込める音の数 (= `snd_config.max_user_buffers`)
  - `live_link` / `live_link_port` (0 = Sonus の既定)
- `Engine::initialize()` — `snd_config_init` → `output_kind`、`max_user_buffers`、
  `max_pitch_ratio = SND_MAX_PITCH_RATIO`、ライブリンク → `snd_create`。失敗は `false` + ログ
  (デバイスが無ければ `SND_ERR_NO_DEVICE` と、`Output::Silent` で動かせることを出す)
- `Engine::load_sound(path)` — WAV を読んでメモリに展開し、Sonus の static user buffer を 1 つ作る
  (初期化したスレッドならその場で。他のスレッドなら次の `update()` で)。失敗は `INVALID_SOUND` + 1 行のログ
- `Engine::play(handle, volume, pitch)` — 範囲の丸めは FMOD 版と同じ (volume 0..1、pitch は 0.01 以上)。
  再生待ちの列に積む (どのスレッドからでも)
- `Engine::update()` — 初期化したスレッドから毎フレーム
- `sonus::advance_output(periods)` — `Output::Manual` のとき、初期化したスレッドから。`periods` 周期
  (1 周期 = Sonus のブロック 256 フレーム) を今鳴らす (`snd_advance_output`)。先に `update()` を呼ぶ
- `Engine::unload_sound` / `shutdown()`

### 出力

- 戻り値は FMOD 版と同じ (`bool` / `SoundHandle`、`INVALID_SOUND` = 0)。`backend()` = `Backend::Sonus`
  (列挙の末尾に追加)、`backend_name()` = `"Sonus"`
- `sonus::stats()`: `running` / `plays_active` (鳴っている再生) / `sounds_held` (中身を持っているバッファ。
  外して解放待ちのものを含む) / `blocks_rendered` / `master_peak` (マスターのメーター) /
  `output_peak` (`Manual` で `advance_output()` が受け取った出力の最大値。`reset_output_peak()` で 0) /
  `live_link_port` / `dropped_plays`
- ログは stderr の `[audio:sonus] ...` (FMOD 版の `[audio:fmod]` と同じ形)

### タスク

- **音と再生**: 音 1 つにつき Sonus の static user buffer を 1 つ作り (元のレートのまま)、鳴らすたびに
  `snd_play_user_buffer_ex(sys, buffer, master, volume, pitch, ...)` を呼ぶ (Sonus D-104)。波形は写さない。
  1 つの音を何本でも同時に、それぞれの音量・ピッチで鳴らせる。ピッチは元のレート ÷ 内部レートと掛け合わされ、
  Sonus がリサンプルする。再生の終わりは `on_end` で知る。
- **外す**: `unload_sound` は Sonus のバッファを destroy する。その音の鳴っている再生は止まる (FMOD の
  `Sound::release` と同じ)。中身は `snd_get_user_buffer_released` が 1 になった `update()` で捨てる。
  それまでは `max_loaded_sounds` に数える。
- **途中の変更**: `Engine` には鳴っている再生の音量・ピッチを変える API が無い (FMOD 版も fire-and-forget)。
  Sonus の `snd_set_playback_volume` / `snd_set_playback_pitch` は使っていない。足すときはそこにつなぐ。
- **スレッド**: `snd_create` を呼んだスレッドが Sonus の所有スレッドになる。`update()` / `shutdown()` /
  `advance_output()` を別のスレッドから呼ぶと、ログに出して何もしない (Sonus の `SND_ERR_WRONG_THREAD` を
  起こさない)。`play()` / `load_sound()` / `unload_sound()` はどのスレッドからでもよい (Sonus への記録は
  所有スレッドの `update()` がまとめて行う)。
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
| 同時に `max_loaded_sounds` を超える音の読み込み | `INVALID_SOUND` (別スレッドで読んだ音は、次の `update()` でその音を外し、以後の `play` は何もしない) | `sound N not loaded: no free Sonus user buffer (...)` |
| Sonus の再生のピッチの範囲 [1/16, 16] の外の pitch (`play` の下限は契約どおり 0.01) | 範囲の端で鳴らす (丸め) | `pitch X is outside Sonus' playback pitch range [1/16, 16]; clamped` |
| 元のレート × pitch ÷ 48 kHz が Sonus のリサンプルの上限 (`SND_MAX_PITCH_RATIO` = 32) を超える | 上限で鳴らす (Sonus が丸める) | `pitch X of a N Hz sound exceeds Sonus' resampling limit ...; clamped` |
| Sonus の再生の枠 (`max_playbacks`) が尽きた `play` | その再生を捨てる (`dropped_plays` が増える) | `play dropped: no free Sonus playback slot` |
| 出力デバイスが無い環境の `initialize()` (`Output::Platform`) | `false` | `snd_create failed: SND_ERR_NO_DEVICE (no output device; sonus::Output::Silent runs without one)` |
| 初期化したスレッド以外からの `update()` / `shutdown()` / `advance_output()` | 何もしない (`advance_output` は `false`) | `update() must run on the thread that called initialize(); ignored` など (各 1 回) |

FMOD 版でも無いもの (3D、ストリーム、DSP、イベント、再生中の音量・ピッチの変更) は、どのバックエンドでも
`Engine` に API が無い。

### Sonus 側で解消した課題 (2026-11-19、Sonus 契約 1.2 の D-102〜D-104)

最初の組み込み (LUDIARS/Ergo#99 の最初のコミット) では Sonus の C API に足りないものがあり、Ergo 側で回避していた。
Sonus が次を入れたので、回避は外した。

| 課題 | 以前の回避 (外した) | 今 |
|---|---|---|
| C API から無音の出力を選べない | Ergo の自前の無音デバイス (`snd/ext.h` の関数表 + タイマースレッド、`src/audio/sonus/silent_output_device`) | `Output::Silent` = `SND_OUTPUT_SILENT`、テストは `Output::Manual` = `SND_OUTPUT_MANUAL` + `advance_output()` (D-102) |
| 既定のファイルシステムが作業ディレクトリの相対しか読めない (Windows では何も開けない) | Studio のテストがバンクを作業ディレクトリの下に置き、相対パスで読ませた | テストのゲームは `file_system_root` = 一時ディレクトリ (D-103)。ergo_audio はバンクを読まないので関係しない |
| user buffer の再生に音量・ピッチが無い | 再生のたびに音量を掛けた波形を写し、`sample_rate` を変えてピッチの代わりにした (`src/audio/sonus/play_buffers`) | 音ごとに 1 つのバッファ + `snd_play_user_buffer_ex` (D-104)。`max_concurrent_plays` は `max_loaded_sounds` に変えた |

### プラットフォーム別タスク

| 環境 | 出力 (`Output::Platform`) | 備考 |
|---|---|---|
| Linux | ALSA (Sonus が `find_package(ALSA)` で見つけたとき)。無ければ Sonus の無音タイマー | 確認済み (gcc / clang、`Output::Manual` / `Silent`) |
| Windows (MinGW) | WASAPI | 新しいファイルの構文検査だけ (MinGW の g++) |
| Windows (MSVC cl / clang-cl) | — | Sonus がビルドできないので **構成エラー** (`cmake/ErgoSonus.cmake`) |
| macOS | Core Audio | 未確認 |

## テスト

`tests/audio/test_ergo_audio_sonus.cpp` (`ERGO_AUDIO_BACKEND=sonus` の構成でだけ登録)。外のファイル・
デバイスに頼らない: WAV はテストが一時ディレクトリに作り、出力は `Output::Manual` (テストが周期を進めるので、
再生の長さが周期の数で決まり、実時間を待たない)。

- WAV の読み取り: 8 / 16 / 24 / 32 bit PCM、float、EXTENSIBLE が値どおり。OGG・圧縮 WAV・12 bit・
  9 チャンネル・サンプル無し・途中で切れたファイル・無いファイルは理由付きの失敗
- `configure()`: 範囲外・知らない出力を拒否、初期化中は拒否して設定を変えない。止まっているときの `advance_output()` は `false`
- `Output::Silent`: 手で進めなくても Sonus のスレッドがブロックを描く。`advance_output()` は `false`
- 再生: 音量 0.5 で出力の最大値が半分、範囲外の音量は FMOD 版と同じ丸め、1 つの音の 3 本同時 (各 0.25) で
  出力が 0.75 倍、バッファは 1 つのまま。`unload_sound` で鳴っている再生が止まり、バッファが解放される
- pitch: 2.0 で長さが半分、0.5 で倍、0.01 は 1/16 に丸めて 16 倍 (Sonus に渡した周期の数で測る)
- 失敗: 未初期化の `load_sound`、WAV 以外、無いファイル、`max_loaded_sounds` を超える読み込み (外して解放されれば
  また読める)、同じ音の再生は上限に数えない、知らないハンドル
- スレッド: 別スレッドの `load_sound()` / `play()` は所有スレッドの次の `update()` で効き、別スレッドの `update()` /
  `shutdown()` / `advance_output()` は効かない。別スレッドの `unload_sound()` も次の `update()` で効く
- ライブリンク (Sonus が `SND_LIVELINK=ON` のとき): 指定したポートで TCP を受け付ける

`tests/audio/test_ergo_audio.cpp` (どのバックエンドでも同じ契約のテスト) も Sonus 版で通る
(Sonus 版では最初に `Output::Silent` を設定する)。
