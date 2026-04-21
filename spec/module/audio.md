# Audio モジュール定義

## 位置付け

**`ergo_audio` はゲーム SE / BGM のための SDK ファサード**。`ergo_sound`
(コア音響エンジン) の後段に置くのではなく、**横並びの独立ライン**として
運用する。両者の関係は以下の通り:

- `ergo_sound` = Ergo のコア柱 (レンダリングと同格の主要実装)。自前実装。
- `ergo_audio` = FMOD 等プロ向け SDK への抽象。ゲーム FX 用の便利ライン。

詳しくは `spec/module/sound.md` の「位置付け (Core Pillar)」節を参照。
本モジュールは **ミドルウェア / SDK 差替えに焦点を当てた** 薄いラッパで
あり、音響表現の新規機能は原則 `ergo_sound` 側に実装する。

## 概要

ゲーム効果音/BGM の再生を担当する **バックエンド抽象ラッパー**。既定の
バックエンドは **FMOD Core** (proprietary SDK, 無償インディー利用枠で
カバー可)。SDK が環境に無いときは自動的に **Dummy バックエンド** に
フォールバックして no-op 動作 (ビルドは通り、ログのみ残す)。

`ergo_sound` が自前デコーダ/ミキサの **コア音響エンジン** なのに対し、
本モジュールは **プロ向け SE 管理の本番ライン**。数百音同時 / 3D
ポジショナル / DSP チェーン / イベントベース再生を将来的に担う。両者は
**用途で棲み分ける** — 音楽ゲーム / BPM 同期 / DSP 研究は `ergo_sound`、
ゲーム SE / BGM / FMOD イベントバンクは `ergo_audio`。

## カテゴリ
システム

## 所属ドメイン
オーディオ / ゲーム FX

## 必要なデータ
- サウンドファイル (WAV / OGG / FMOD FSB 等、FMOD がデコードする形式)
- ホスト側のボリューム / ピッチパラメータ
- (将来) 3D ポジション、ミキサグループ、FMOD Studio イベントバンク

## 依存
- C++17 標準ライブラリ
- **FMOD Core API** (`fmod.hpp`, `fmodL.lib` / `fmod.lib`, `fmod.dll`)
  — `FMOD_SDK_DIR` 環境変数または `cmake -DFMOD_SDK_DIR=<path>` で位置指定
- 無い場合は **Dummy バックエンド** に自動切替 (ビルド成立)
- (テスト) mini-gtest

## 変数
- `Engine` シングルトン
  - `backend_name_` : "FMOD" | "Dummy"
  - `initialized_`   : bool
  - `fmod_system_`   : `FMOD::System*` (FMOD バックエンドのみ)
  - サウンドハンドル → 内部リソースへのマップ
- サウンドハンドル (uint32_t, 0 = invalid)
- チャンネル (現状 one-shot 再生のみ)

## 作業

### 入力
- `Engine::initialize()` — バックエンド選択 + FMOD::System_Create 相当
- `Engine::load_sound(path)` — WAV/OGG をロードしハンドルを返す
- `Engine::play(handle, vol, pitch)` — one-shot 再生
- `Engine::update()` — 毎フレーム呼び出し (FMOD のコールバック処理)
- `Engine::shutdown()`

### 出力
- 成否 `bool` / ハンドル `SoundHandle`
- `backend_name()` でランタイムに現在のバックエンドを問い合わせ可能

### タスク
- FMOD バックエンド:
  - `FMOD::System` 作成 → `init(128, FMOD_INIT_NORMAL)`
  - サウンドは `FMOD_DEFAULT` (メモリロード)、必要なら `FMOD_CREATESTREAM`
  - play はチャンネル ID を管理せず fire-and-forget (one-shot)
- Dummy バックエンド:
  - ロードはハンドル連番発行、再生はログ出力のみ
- ダミープラグ (リンクのみ用) は本モジュールでは不要 (Dummy が常時提供)

## テスト (Dummy バックエンド)

- `initialize` / `shutdown` のライフサイクル
- 存在しないパスへの `load_sound` は 0 (invalid) を返す (Dummy はパス
  検証を行わず成功させる — FMOD バックエンドはファイル存在チェック)
- `play(INVALID)` は早期 return で安全
- `update()` は複数回呼んでも安全
- `backend_name()` が "Dummy" を返す (SDK 無し環境)

## FMOD 利用条件 (ライセンス)

FMOD は **プロプライエタリ**。無償利用枠:
- 個人 / インディー: 年間売上 $200k 未満
- 配布物にクレジット表記 (`docs/third_party/FMOD.md` に記載)

本リポジトリは FMOD SDK を **同梱しない**。ユーザが別途 firelight.com
からダウンロードし、パスを通す運用:

```
# Windows
setx FMOD_SDK_DIR "C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows"
```

CMake 再構成後は FMOD バックエンドが自動選択される (`ergo_audio: using
FMOD 2.x at ...` がログに出る)。
