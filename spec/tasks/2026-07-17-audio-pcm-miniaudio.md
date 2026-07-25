---
task: audio-pcm-miniaudio
project: Ergo
kind: 実装
status: delegated
created: 2026-07-17T00:00:00.000Z
source_session: lictor-fa3f272d-5b1a-4d97-9c6b-28bc55518355
memoria_task_id: 541
actio_task_id: null
memory_links:
  - E:/Document/Ars/Figmentum/spec/plan/audio/library-selection.md
  - E:/Document/Ars/Figmentum/spec/plan/audio/task-07-runtime-playback.md
  - E:/Document/Ars/Figmentum/spec/feature/audio-synth.md
---

# ergo_audio: PCM 再生 API (`load_sound_pcm`) + miniaudio バックエンド追加

## 目的

Figmentum が実行時に合成する効果音 (float mono PCM、`renderSound` の出力) を
ゲーム内で再生できるようにする。現状の `ergo_audio` はファイル再生のみで、
FMOD SDK が無い環境 (この開発機・CI) では Dummy = 無音。
選定結論 (`memory_links` の library-selection.md、案 A) に従い、
**PCM API を全バックエンドに追加**し、**miniaudio (MIT-0、単一ヘッダ) を
第 3 バックエンド**として導入して FMOD 無し環境でも実音を出す。

## 完了条件

1. `include/ergo/audio/audio_engine.h` に追加 (C++17、既存 API・既存コメント様式を維持):
   ```cpp
   /// メモリ上の float mono PCM からサウンドを作る (Figmentum 等の動的合成波形用)。
   /// samples はコピーされ、呼び出し後に解放してよい。失敗 / 未初期化は 0。
   SoundHandle load_sound_pcm(const float* samples, size_t sampleCount, int sampleRate);
   ```
   + 末尾の free-function ショートカット群にも同名 inline を追加。
2. **FMOD バックエンド** (`src/audio/audio_engine_fmod.cpp`):
   `FMOD_CREATESOUNDEXINFO` (`cbsize` / `length = sampleCount*sizeof(float)` /
   `numchannels = 1` / `defaultfrequency = sampleRate` /
   `format = FMOD_SOUND_FORMAT_PCMFLOAT`) + フラグ
   `FMOD_OPENMEMORY | FMOD_OPENRAW | FMOD_2D | FMOD_LOOP_OFF` で `createSound`。
3. **Dummy バックエンド** (`src/audio/audio_engine_dummy.cpp`):
   既存 `load_sound` と同様にハンドル発番 + 「what would have played」ログ
   (サンプル数と sampleRate を出す)。
4. **miniaudio バックエンド** (`src/audio/audio_engine_miniaudio.cpp` 新規):
   - miniaudio は **FetchContent** で取得 (リポ https://github.com/mackron/miniaudio、
     タグ or commit を **pin** する。サブモジュール/ベンダリング禁止)。
     `#define MINIAUDIO_IMPLEMENTATION` はこの .cpp 1 箇所のみ。
   - `ma_engine` を `initialize()` で init (失敗したら false を返し以後 no-op =
     既存 Engine 契約と同じ)。`shutdown()` で uninit。
   - `load_sound(path)`: パスを登録 (存在チェック)。`play()` 時に
     `ma_sound_init_from_file` + volume (`ma_sound_set_volume`) +
     pitch (`ma_sound_set_pitch`) + `ma_sound_start`。
   - `load_sound_pcm`: サンプルを**コピーして保持**。`play()` 時に保持データから
     `ma_audio_buffer` (再生ごとに個別インスタンス — 読み取りカーソルを共有しない)
     → `ma_sound_init_from_data_source`。
   - 再生中インスタンスは active リストで管理し、`update()` で
     `ma_sound_at_end` のものを uninit + 解放 (リソース寿命規約: 確保した所有者が
     全経路で解放)。`shutdown()` で全部解放。
5. **CMake**: バックエンド自動選択を `fmod → miniaudio → dummy` へ。
   `ERGO_AUDIO_BACKEND` の選択肢に `miniaudio` を追加
   (明示指定で FetchContent 失敗なら **configure エラー** — 無言で dummy に
   落とさない。RULE_CODE §7.1)。`ERGO_AUDIO_BACKEND_MINIAUDIO=1` を定義。
   configure ログに `ergo_audio: backend = miniaudio` を出す。
6. `THIRD_PARTY_NOTICES.md` に miniaudio (MIT-0 / public domain デュアル) を追記。
7. **テスト** (`tests/audio/`、既存 test_audio_engine の様式・gtest):
   - `load_sound_pcm` は `is_initialized()` が true なら非 0 ハンドルを返し、
     `play(handle)` がクラッシュしない。false なら 0 を返す
     (CI はヘッドレスでデバイス init に失敗し得る — **どちらの分岐でも pass する**
     デバイス非依存のアサーションにする)。
   - `unload_sound` 後の `play` が安全 (no-op)。
   - 既存テストがすべて通ること。

## スコープ (編集可ディレクトリ)

- `include/ergo/audio/` / `src/audio/` / `tests/audio/`
- `CMakeLists.txt` (ergo_audio 節のみ) / `THIRD_PARTY_NOTICES.md` / `module_list.md` (必要なら)
- この task md 自体 (`spec/tasks/2026-07-17-audio-pcm-miniaudio.md`) を実装ブランチに含めてよい

## 自己検証チェックリスト (PR 説明に結果を書くこと)

- [ ] `grep -rn "load_sound_pcm" include/ergo/audio src/audio | wc -l` ≥ 5 (ヘッダ宣言 + inline + 3 バックエンド実装)
- [ ] `grep -n "MINIAUDIO_IMPLEMENTATION" src/audio/audio_engine_miniaudio.cpp` = 1 箇所のみ (リポ全体でも 1)
- [ ] `grep -n "ma_engine_init\|ma_sound_init_from_data_source\|ma_audio_buffer" src/audio/audio_engine_miniaudio.cpp` が全部ヒット (スタブでない)
- [ ] FMOD SDK 無しで `cmake -B build_task` のログに `ergo_audio: backend = miniaudio`
- [ ] `ctest` の audio 系テストが全部 pass (ローカル + CI)
- [ ] 既存 API 互換: `AdventureCube` / `PrivateGame` 側の変更ゼロで再リンク可能 (シグネチャ変更なし)

## 進め方 (リポ規約)

- ブランチ: `feat/audio-pcm-miniaudio` を **origin/main** から (Ergo に develop は無い)。
  main 直 push 禁止。1 PR (squash mergeable)。
- **worktree ではサービス起動・動作テストをしない** (単体テスト = ctest は可)。
  実音の耳確認は Cc の confirm キュー / 人間が行う。
- C++17。テストは vitest ではなく **gtest + ctest** (このリポの既定)。
- 完了したら PR URL を報告。
