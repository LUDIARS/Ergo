# Ergo モジュール一覧

Ergo リポジトリに含まれる C++17 モジュールの一覧。機械処理向けの
等価な一覧は `module_list.yaml` にある。新規モジュールを追加したら
**両方** にエントリを足すこと (CLAUDE.md の「新規モジュール追加手順」
参照)。

| 名前 | 概要 | カテゴリ | 公開ヘッダ | 実装 | 仕様書 | 付随ツール |
|---|---|---|---|---|---|---|
| `ergo_input`        | マウス・キーボード・ゲームパッド・USB HID の統一入力レイヤ (inject API でテスト可能) | システム | `include/ergo/input/`        | `src/input/`        | `spec/module/input.md`        | — |
| `ergo_inspector`    | in-process POSIX WS サーバでホスト変数をライブ編集 (Phase 2 で tools/ergo 側へ統合予定) | システム | `include/ergo/inspector/`    | `src/inspector/`    | `spec/module/inspector.md`    | (内蔵 UI, 将来 `inspector` プラグイン) |
| `ergo_particle`     | CPU パーティクル sim + Pictor Vulkan ビルボード renderer (optional)                   | システム | `include/ergo/particle/`     | `src/particle/`     | `spec/module/particle.md`     | `tools/ergo/` (`particle` plugin) |
| `ergo_gpu_particle` | Shuriken 相当の GPU compute パーティクル。`IGpuBackend` 抽象で Vulkan/WebGPU 両対応   | システム | `include/ergo/gpu_particle/` | `src/gpu_particle/` | `spec/module/gpu_particle.md` | — |
| `ergo_bind`         | 任意ホスト変数を WS で外部エディタに公開 (`BIND_VAR`). アウトバウンド接続             | システム | `include/ergo/bind/`         | `src/bind/`         | `spec/module/bind.md`         | `tools/ergo/` (`variable` plugin) |
| `ergo_sound`        | WAV デコード + ストリーミング + ミキサ + 波形処理 + Quantizer (外部 lib 無し)          | システム | `include/ergo/sound/`        | `src/sound/`        | `spec/module/sound.md`        | — |
| `ergo_frame`        | アプリ起動からの累計フレーム数 + rolling FPS + HUD 文字列                              | システム | `include/ergo/frame/`        | `src/frame/`        | `spec/module/frame.md`        | — |
| `ergo_log`          | 4-level ロガー (Error/Warn/Info/Debug)。行頭にフレーム番号を埋め込む                   | システム | `include/ergo/log/`          | `src/log/`          | `spec/module/log.md`          | — |

共通ユーティリティ:

| 名前 | 概要 |
|---|---|
| `ergo_common` | モジュール横断の小物ユーティリティ。現状は `ergo::common::jsonm` (minimal JSON codec) のみ。inspector/bind から PUBLIC link されて transitive に取り込まれる |

## 状態メモ

- **`ergo_inspector`**: `spec/tool/ergo.md` Phase 2 で記述した通り、現状は
  内蔵 POSIX WS サーバを持つが将来 `tools/ergo/` の `inspector` プラグイン
  に移行予定。Windows は dummy 実装のため事実上使えない。
- **`ergo_particle::Renderer`**: Pictor + Vulkan が利用側に揃っている
  ときのみビルド (`ERGO_PARTICLE_HAS_RENDERER=ON`)。sim (`ParticleSystem`)
  だけならいつでも使える。
- **`ergo_gpu_particle`**: 純粋 C++ モジュール。GPU 呼び出しは `IGpuBackend`
  経由でホスト統合層 (Vulkan / WebGPU) に委譲する。glslc があれば
  compute シェーダを SPIR-V に焼く。
- **`ergo_sound` は WAV 専用**: 自前デコーダのみ抱える方針。OGG Vorbis 等の
  圧縮フォーマットが必要になった場合は `third_party/` に stb_vorbis を
  vendor して別コミットで足す。現状は `.ogg` 拡張子を渡しても
  `IAudioDecoder::create` が nullptr を返す。

## Web ツール

全ての Web 系開発者ツールは **`tools/ergo/`** に統合済み。新規ツールは
単独パッケージではなく `tools/ergo/src/plugins/<id>/` に追加する
(`spec/tool/ergo.md` 参照)。現行プラグイン:

- `particle` — 旧 `tools/particle-editor/` の移植
- `variable` — 旧 `tools/variable-editor/` の移植
