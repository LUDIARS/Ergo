# Ergo モジュール一覧

Ergo リポジトリに含まれる C++17 モジュールの一覧。機械処理向けの
等価な一覧は `module_list.yaml` にある。新規モジュールを追加したら
**両方** にエントリを足すこと (CLAUDE.md の「新規モジュール追加手順」
参照)。

| 名前 | 概要 | カテゴリ | 公開ヘッダ | 実装 | 仕様書 | 付随ツール |
|---|---|---|---|---|---|---|
| `ergo_input`        | マウス・キーボード・ゲームパッド・USB HID の統一入力レイヤ (inject API でテスト可能) | システム | `include/ergo/input/`        | `src/input/`        | `spec/module/input.md`        | — |
| `ergo_particle`     | CPU パーティクル sim + Pictor Vulkan ビルボード renderer (optional)                   | システム | `include/ergo/particle/`     | `src/particle/`     | `spec/module/particle.md`     | `tools/ergo/` (`particle` plugin) |
| `ergo_gpu_particle` | Shuriken 相当の GPU compute パーティクル。`IGpuBackend` 抽象で Vulkan/WebGPU 両対応   | システム | `include/ergo/gpu_particle/` | `src/gpu_particle/` | `spec/module/gpu_particle.md` | — |
| `ergo_bind`         | 任意ホスト変数を WS で外部エディタに公開 (`BIND_VAR`). アウトバウンド接続             | システム | `include/ergo/bind/`         | `src/bind/`         | `spec/module/bind.md`         | `tools/ergo/` (`variable` plugin) |
| `ergo_actor`        | シーングラフ基底クラス。ツリーノード自動登録 + Actor 配下に変数を公開 (`ergo_bind` に委譲) | システム | `include/ergo/actor/`        | `src/actor/`        | `spec/module/actor.md`        | `tools/ergo/` (`variable` plugin で表示) |
| `ergo_cast`         | Actor を名前付き Scene に束ねる軽量グルーピング層 (一括/個別 activate + snapshot)。非所有・描画非依存。`ergo_scene` (look-dev ドキュメント) とは別物 | システム | `include/ergo/cast/`         | `src/cast/`         | `spec/module/cast.md`         | — |
| `ergo_sound`        | WAV デコード + ストリーミング + ミキサ + 波形処理 + Quantizer (外部 lib 無し)          | システム | `include/ergo/sound/`        | `src/sound/`        | `spec/module/sound.md`        | — |
| `ergo_audio`        | ゲーム SE/BGM の SDK ファサード (FMOD Core 既定 + Dummy 自動フォールバック)。`ergo_sound` とは独立・横並び | システム | `include/ergo/audio/`        | `src/audio/`        | `spec/module/audio.md`        | — |
| `ergo_frame`        | アプリ起動からの累計フレーム数 + rolling FPS + HUD 文字列                              | システム | `include/ergo/frame/`        | `src/frame/`        | `spec/module/frame.md`        | — |
| `ergo_profile`      | パフォーマンス確認用タイムライン。 AOP 的マーカー注入で速度/メモリ/スレッドを計測し Chrome Trace 形式で出力 | システム | `include/ergo/profile/`      | `src/profile/`      | `spec/module/profile.md`      | `tools/ergo/` (`profile` plugin) |
| `ergo_log`          | 4-level ロガー (Error/Warn/Info/Debug)。行頭にフレーム番号を埋め込む                   | システム | `include/ergo/log/`          | `src/log/`          | `spec/module/log.md`          | — |
| `ergo_io`           | 最小ファイル I/O ラッパー (`<filesystem>` の薄膜、UTF-8 `std::string` API)             | システム | `include/ergo/io/`           | `src/io/`           | `spec/module/io.md`           | — |
| `ergo_world_time`   | グローバル time-scale コンポーザ (HitStop / HitSlow)。Foundation/WorldTimeScale の C++ ポート | システム | `include/ergo/world_time/`   | `src/world_time/`   | `spec/module/world_time.md`   | — |
| `ergo_blackboard`   | グローバル名前付き Property レジストリ (subscribe + カテゴリ lifecycle)。Foundation/Blackboard の C++ ポート | システム | `include/ergo/blackboard/`   | `src/blackboard/`   | `spec/module/blackboard.md`   | — |
| `ergo_ui`           | SVG ラスタ + 9-slice フレームコンポーザ (RGBA8, Pictor/Vulkan 依存なし)                | UI       | `include/ergo/ui/`           | `src/ui/`           | `spec/module/ui.md`           | — |
| `ergo_custos`       | Custos 遠隔テストランナーと話す in-process HTTP ブリッジ (`/health` `/screenshot` `/key`) | システム | `include/ergo/custos/`       | `src/custos/`       | `spec/module/custos.md`       | (外部: `LUDIARS/Custos`) |
| `ergo_health`       | HP コンテナ + ダメージ/回復/死亡コールバック (game-lexicon: `health-system`) | ロジック | `include/ergo/health/`       | `src/health/`       | `spec/module/health.md`       | — |
| `ergo_score`        | スコアカウンタ + コンボ倍率 + ハイスコア通知 (game-lexicon: `score-system`)  | ロジック | `include/ergo/score/`        | `src/score/`        | `spec/module/score.md`        | — |
| `ergo_combo_counter`| 連続成功カウンタ + フルコンボ通知 (game-lexicon: `combo-counter`)            | ロジック | `include/ergo/combo_counter/`| `src/combo_counter/`| `spec/module/combo_counter.md`| — |
| `ergo_timing_judge` | 音ゲー用 ms タイミング判定 PERFECT/GREAT/GOOD/MISS (game-lexicon: `timing-judge`) | ロジック | `include/ergo/timing_judge/` | `src/timing_judge/` | `spec/module/timing_judge.md` | — |
| `ergo_character`    | TCC 流 Character Controller (Brain/Check/Control/Effect 合成 + 優先度切替)。エンジン非依存 | ゲームオブジェクト | `include/ergo/character/` | `src/character/` | `spec/module/character.md` | — |
| `ergo_math`         | C++20 concepts ベースの汎用数学ライブラリ。Vec/Mat (テンプレート汎用)、SoA バッチ演算 (SSE2 optional)、Arena バンプアロケータ、ObjectPool (連続スラブ + free-list) | システム | `include/ergo/math/` | `src/math/` | `spec/module/math.md` | — |
| `ergo_physics2d`    | 2D 剛体物理エンジン (Box2D ライク)。Shape(Circle/Polygon)/Body/World/ContactEvent。sequential impulse ソルバ + SAT narrowphase + N² AABB broadphase。ergo_math の ObjectPool で DoD 配置 | システム | `include/ergo/physics2d/` | `src/physics2d/` | `spec/module/physics2d.md` | — |
| `ergo_render`       | Pictor 横断オーケストレーション層。`RenderContext` / `IRenderLayer` / `FrameComposer` / `ScreenshotBridge` / カメラ math / `StageRenderer` | システム | `include/ergo/render/`       | `src/render/`       | `spec/module/render.md`       | — |
| `ergo_ui_layout`    | Data-driven UI layout runtime. JSON model + binding + layout solve + render adapter emit for live-editable HUD/UI | UI | `include/ergo/ui_layout/` | `src/ui_layout/` | `spec/module/ui_layout.md` | `ui_layout` |

| `ergo_vector`       | SVG path polygon tessellation + extrude 3D mesh + vertex animation scene | UI | `include/ergo/vector/`       | `src/vector/`       | `spec/module/vector.md`       | — |

共通ユーティリティ:

| 名前 | 概要 |
|---|---|
| `ergo_common` | モジュール横断の小物ユーティリティ。現状は `ergo::common::jsonm` (minimal JSON codec) のみ。`ergo_bind` から PUBLIC link されて transitive に取り込まれる |

## 状態メモ

- **`ergo_inspector` は廃止済み (2026-04-21)**: 機能調査の結果 `ergo_bind` の
  完全なサブセットと判定され、モジュール自体を廃止。`ergo_bind` に完全吸収
  された (詳細は `spec/module/bind.md` の「歴史的経緯」節 / `spec/tool/ergo.md`
  の「廃止プラン」節)。live tuning の唯一の窓口は `ergo_bind`。
- **`ergo_actor`**: `ergo_bind` に実装を委譲するシーングラフ基底。
  `ERGO_BUILD_ACTOR AND ERGO_BUILD_BIND` の両方が ON のときだけビルドされる。
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
- **`ergo_render`**: Pictor の上・ゲームの下に入る横断オーケストレーション層。
  ゲームの `WorldRenderer` God Class から「初期化順 / パス構成 / フレーム
  ループ / submit/present / screenshot / 破棄順」だけを引き取る。Vulkan
  非依存部分 (カメラ math / asset path / `ScreenshotBridge` / `FrameComposer`
  のパス列管理) は常時ビルド。Vulkan 実描画経路 (`FrameComposer::run_frame`、
  `StageRenderer` の pipeline 構築) は `pictor` ターゲット + Vulkan SDK が
  揃ったときだけ有効化される (`ERGO_RENDER_HAS_VULKAN`)。ゲーム固有の
  サブレンダラ実装本体・actor→drawable 変換・パス列の組み立ては
  `ergo_render` に入れずゲーム側に残す。

## Web ツール

全ての Web 系開発者ツールは **`tools/ergo/`** に統合済み。新規ツールは
単独パッケージではなく `tools/ergo/src/plugins/<id>/` に追加する
(`spec/tool/ergo.md` 参照)。現行プラグイン:

- `particle` — 旧 `tools/particle-editor/` の移植
- `variable` — 旧 `tools/variable-editor/` の移植
