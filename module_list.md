# Ergo モジュール一覧

Ergo リポジトリに含まれる C++ モジュールの一覧。C++17 を基準とし、
`ergo_math` / `ergo_physics2d` だけは C++20 (`target_compile_features` で
利用側へ伝わる)。機械処理向けの等価な一覧は [`module_list.yaml`](module_list.yaml)
にあり、所属ドメイン・リンクする `ergo_*` ターゲット (`depends`)・付随
ターゲット (`extra_targets`)・プラグイン id (`tool_plugins`) はそちらが正本。
新規モジュールを追加したら **両方** にエントリを足すこと (CLAUDE.md の
「新規モジュール追加手順」参照)。両者と CMake オプション・`include/ergo/`・
`tools/ergo` のプラグイン・README の食い違いは ctest `test_module_catalog`
(`tests/test_module_catalog.cmake`) が検出する。

| 名前 | 概要 | カテゴリ | 公開ヘッダ | 実装 | 仕様書 | 付随ツール |
|---|---|---|---|---|---|---|
| `ergo_input`        | マウス・キーボード・ゲームパッド・USB HID の統一入力レイヤ (inject API でテスト可能) | システム | `include/ergo/input/`        | `src/input/`        | `spec/feature/module/input.md`        | — |
| `ergo_particle`     | CPU パーティクル sim + Pictor Vulkan ビルボード renderer (optional)。`particle` プラグインから設定を受け取るフィーダーは任意ターゲット `ergo_particle_link` (`ERGO_BUILD_PARTICLE_LINK`) | システム | `include/ergo/particle/`     | `src/particle/`     | `spec/feature/module/particle.md`     | `tools/ergo/` (`particle` / `vfx-suggest` / `vfx-breed` plugin) |
| `ergo_gpu_particle` | Shuriken 相当の GPU compute パーティクル。`IGpuBackend` 抽象で Vulkan/WebGPU 両対応   | システム | `include/ergo/gpu_particle/` | `src/gpu_particle/` | `spec/feature/module/gpu_particle.md` | — |
| `ergo_bind`         | 任意ホスト変数を WS で外部エディタに公開 (`BIND_VAR`). アウトバウンド接続             | システム | `include/ergo/bind/`         | `src/bind/`         | `spec/feature/module/bind.md`         | `tools/ergo/` (`variable` plugin) |
| `ergo_actor`        | シーングラフ基底クラス。ツリーノード自動登録 + Actor 配下に変数を公開 (`ergo_bind` に委譲) | システム | `include/ergo/actor/`        | `src/actor/`        | `spec/feature/module/actor.md`        | `tools/ergo/` (`variable` plugin で表示) |
| `ergo_cast`         | Actor を名前付き Scene に束ねる軽量グルーピング層 (一括/個別 activate + snapshot)。非所有・描画非依存。`ergo_scene` (look-dev ドキュメント) とは別物 | システム | `include/ergo/cast/`         | `src/cast/`         | `spec/feature/module/cast.md`         | — |
| `ergo_scene`        | 保存できるルックデヴ用シーン文書 (`*.scene.json` の CRUD・パッチ適用・カメラ行列・`SceneGraph`)。`scene_editor` プラグインとのライブ編集は任意ターゲット `ergo_scene_link` (`ERGO_BUILD_SCENE_LINK`) | システム | `include/ergo/scene/`        | `src/scene/`        | `spec/feature/module/scene.md`        | `tools/ergo/` (`scene_editor` plugin) |
| `ergo_sound`        | WAV デコード + ストリーミング + ミキサ + 波形処理 + Quantizer (外部 lib 無し)          | システム | `include/ergo/sound/`        | `src/sound/`        | `spec/feature/module/sound.md`        | — |
| `ergo_audio`        | ゲーム SE/BGM の SDK ファサード (FMOD Core 既定 + Dummy 自動フォールバック)。`ergo_sound` とは独立・横並び | システム | `include/ergo/audio/`        | `src/audio/`        | `spec/feature/module/audio.md`        | — |
| `ergo_frame`        | アプリ起動からの累計フレーム数 + rolling FPS + HUD 文字列                              | システム | `include/ergo/frame/`        | `src/frame/`        | `spec/feature/module/frame.md`        | — |
| `ergo_profile`      | パフォーマンス確認用タイムライン。 AOP 的マーカー注入で速度/メモリ/スレッドを計測し Chrome Trace 形式で出力 | システム | `include/ergo/profile/`      | `src/profile/`      | `spec/feature/module/profile.md`      | `tools/ergo/` (`profile` plugin) |
| `ergo_log`          | 4-level ロガー (Error/Warn/Info/Debug)。行頭にフレーム番号を埋め込む。出力先はシンクで差し替え、時刻源は注入可能。`log` プラグインへ行を流す `LiveSink` は任意ターゲット `ergo_log_live` (`ERGO_BUILD_LOG_LIVE`) | システム | `include/ergo/log/`          | `src/log/`          | `spec/feature/module/log.md`          | `tools/ergo/` (`log` plugin) |
| `ergo_io`           | 最小ファイル I/O ラッパー (`<filesystem>` の薄膜、UTF-8 `std::string` API)             | システム | `include/ergo/io/`           | `src/io/`           | `spec/feature/module/io.md`           | — |
| `ergo_world_time`   | グローバル time-scale コンポーザ (HitStop / HitSlow)。Foundation/WorldTimeScale の C++ ポート | システム | `include/ergo/world_time/`   | `src/world_time/`   | `spec/feature/module/world_time.md`   | — |
| `ergo_blackboard`   | グローバル名前付き Property レジストリ (subscribe + カテゴリ lifecycle、名前だけで `find<T>` / `subscribe<T>(name, cb)` / `visit`)。Foundation/Blackboard の C++ ポート | システム | `include/ergo/blackboard/`   | `src/blackboard/`   | `spec/feature/module/blackboard.md`   | — |
| `ergo_ui`           | SVG ラスタ + 9-slice フレームコンポーザ (RGBA8, Pictor/Vulkan 依存なし)                | UI       | `include/ergo/ui/`           | `src/ui/`           | `spec/feature/module/ui.md`           | — |
| `ergo_custos`       | Custos 遠隔テストランナーと話す in-process HTTP ブリッジ (`/health` `/screenshot` `/key`) | システム | `include/ergo/custos/`       | `src/custos/`       | `spec/feature/module/custos.md`       | (外部: `LUDIARS/Custos`) |
| `ergo_health`       | HP コンテナ + ダメージ/回復/死亡コールバック (game-lexicon: `health-system`) | ロジック | `include/ergo/health/`       | `src/health/`       | `spec/feature/module/health.md`       | — |
| `ergo_score`        | スコアカウンタ + コンボ倍率 + ハイスコア通知 (game-lexicon: `score-system`)  | ロジック | `include/ergo/score/`        | `src/score/`        | `spec/feature/module/score.md`        | — |
| `ergo_combo_counter`| 連続成功カウンタ + フルコンボ通知 (game-lexicon: `combo-counter`)            | ロジック | `include/ergo/combo_counter/`| `src/combo_counter/`| `spec/feature/module/combo_counter.md`| — |
| `ergo_timing_judge` | 音ゲー用 ms タイミング判定 PERFECT/GREAT/GOOD/MISS (game-lexicon: `timing-judge`) | ロジック | `include/ergo/timing_judge/` | `src/timing_judge/` | `spec/feature/module/timing_judge.md` | — |
| `ergo_character`    | TCC 流 Character Controller (Brain/Check/Control/Effect 合成 + 優先度切替)。エンジン非依存 | ゲームオブジェクト | `include/ergo/character/` | `src/character/` | `spec/feature/module/character.md` | — |
| `ergo_math`         | C++20 concepts ベースの汎用数学ライブラリ (利用側にも C++20 が伝わる)。Vec/Mat (テンプレート汎用)、SoA バッチ演算 (SSE2 optional)、Arena バンプアロケータ、ObjectPool (連続スラブ + free-list) | システム | `include/ergo/math/` | `src/math/` | `spec/feature/module/math.md` | — |
| `ergo_physics2d`    | 2D 剛体物理エンジン (Box2D ライク)。Shape(Circle/Polygon)/Body/World/ContactEvent。sequential impulse ソルバ + SAT narrowphase + N² AABB broadphase。ergo_math の ObjectPool で DoD 配置。C++20 (`ergo_math` から伝わる) | システム | `include/ergo/physics2d/` | `src/physics2d/` | `spec/feature/module/physics2d.md` | — |
| `ergo_resource`     | タグ駆動リソース自動選択 + 読み込み。JSON マニフェスト catalog + Clio 同重みスコアラ + バイト列 loader (MUSA Clio の C++ 移植、ネットワーク非依存) | システム | `include/ergo/resource/` | `src/resource/` | `spec/module/resource.md` | — |
| `ergo_http`         | libcurl バックエンドの同期 HTTP クライアント。`IHttpClient` / `get` / `post`。公開 API は `std::` のみで curl を隠蔽。既定 OFF (`ERGO_WITH_CURL` を強制) | システム | `include/ergo/http/`         | `src/http/`         | `spec/feature/module/http.md`         | (依存: `curl`) |
| `ergo_render`       | Pictor 横断オーケストレーション層。`RenderContext` / `IRenderLayer` / `FrameComposer` / `ScreenshotBridge` / カメラ math / `StageRenderer` | システム | `include/ergo/render/`       | `src/render/`       | `spec/feature/module/render.md`       | — |
| `ergo_tela_graph`   | Ergo 可視化の共通出口。 配置済み関係図 → Tela `TELA_GRAPH 1` ファイル (検査付き) / `tela::Graph` (Tela があるとき) | システム | `include/ergo/tela_graph/` | `src/tela_graph/` | `spec/module/tela_graph.md` | (外部: Tela `tela_view --graph`) |
| `ergo_render_viz`   | Pictor 描画パイプライン (パス・アタッチメント・ポストプロセス・CPU 段・GPU 時間) を関係図にする。 CLI `ergo_render_viz` はプリセット / プロファイル JSON → `.tela` | システム | `include/ergo/render_viz/` | `src/render_viz/` | `spec/module/render_viz.md` | `ergo_render_viz` (CLI、 pictor があるとき) |
| `ergo_graph_layout` | 汎用の層別配置。 抽象グラフ (ノード・グループ・有向辺) → 配置済み関係図 (左 = 使う側、 最長経路の列、 重心法、 縦横の折れ線、 決定的) | システム | `include/ergo/graph_layout/` | `src/graph_layout/` | `spec/module/graph_layout.md` | — |
| `ergo_package_model` | パッケージ構成の値型。 `ergo-packages.resolved.json` と CMake File API codemodel v2 を読み、 帰属・既定の除外・層・宣言漏れ (PKG-17) を出す (読むだけ)。 W6: P4 の入力 (`watch-report.json` と `ergo-packages.status.json`) の読み手 | システム | `include/ergo/package_model/` | `src/package_model/` | `spec/module/package_model.md` | — |
| `ergo_package_scan` | 観測対象 (マニフェストの `observe`) の兄弟チェックアウトから `package.json` の依存・`.gitmodules` と gitlink・`ported_from` / `relations` を根拠 (ファイル・行・キー) 付きの使用関係にする (読むだけ、 宣言された範囲の外を読まない) | システム | `include/ergo/package_scan/` | `src/package_scan/` | `spec/module/package_scan.md` | — |
| `ergo_package_viz`  | パッケージ版一覧 (P1) / ターゲットのリンク図 (P2) / 使用関係図 (P3) / ホスト状態図 (P4、 `--view host`) を上限内の関係図にする (簡約・畳み込み・焦点・分割)。 CLI `ergo_package_viz` と `ergo_package_graphs` | システム | `include/ergo/package_viz/` | `src/package_viz/` | `spec/module/package_viz.md` | `ergo_package_viz` (CLI、 `--check` で PKG-17) |
| `ergo_package_manager` | パッケージマネージャの読み取り層。 ホストの宣言 (`ergo-packages.json` の `host` 節) の読み取りと検査 (PKG-01/02/15)、 漏れ検査、 ブートストラップの生成と古さ、 `init` の計画、 一覧、 書式を保つ書き換え (add / remove / update の pin) と持ち主の判定、 W6: テスト結果の集計 (JUnit / LastTest.log) と `ergo-packages.status.json` の書き手 | システム | `include/ergo/package_manager/` | `src/package_manager/` | `spec/module/package_manager.md` | `ergo_pkg` (CLI: `init` / `list` / `doctor` / `fetch` / `add` / `remove` / `update` / `status`) |
| `ergo_package_watch` | パッケージの更新の検知。 依存の列挙、 取得元の interface (`IRefSource` / `IHistorySource`、 W4 の `IReviewSource` / `IRunSource`)、 tag / track / off-main の判定、 run / review / merged と由来 (pf / ai / human / unknown)、 前回状態と重複の抑止 (`track` の 6 時間畳み込み)、 報告と出来事の書き手 (漏れ検査つき)、 UPM ホスト (Unity プロジェクト) の読み手。 Unity パッケージ `unity/com.ludiars.ergo.packages` が報告を読む | システム | `include/ergo/package_watch/` | `src/package_watch/` | `spec/module/package_watch.md` | `ergo_pkg` (CLI: `outdated` / `watch`)、 Unity: Window > LUDIARS > Ergo Packages |
| `ergo_ui_layout`    | Data-driven UI layout runtime. JSON model + binding + layout solve + render adapter emit for live-editable HUD/UI | UI | `include/ergo/ui_layout/` | `src/ui_layout/` | `spec/feature/module/ui_layout.md` | `ui_layout` |
| `ergo_ui_kit`       | リテインドモードの UI コンポーネントフレームワーク。uidoc.json のツリー → auto-layout / constraints 解決 → GPU 非依存の描画リスト (描画は Pictor `UIRenderer`)。ポインタ入力のバブリングとプロパティアニメ | UI | `include/ergo/ui_kit/` | `src/ui_kit/` | `spec/module/ui_kit.md` | — |
| `ergo_vector`       | SVG path polygon tessellation + extrude 3D mesh + vertex animation scene | UI | `include/ergo/vector/`       | `src/vector/`       | `spec/feature/module/vector.md`       | — |

共通ユーティリティ:

| 名前 | 概要 |
|---|---|
| `ergo_common` | モジュール横断の小物ユーティリティ (仕様は `spec/feature/module/common.md`)。`ergo_common` は `ergo::common::jsonm` (minimal JSON codec) で、ソケットに依存しない。別ターゲット `ergo_common_ws` は共有 WebSocket クライアント (`ergo::common::ws::Client`) で、`ergo_bind` / `ergo_particle_link` / `ergo_scene_link` / `ergo_log_live` が使う。どちらも `ERGO_BUILD_*` を持たず常にビルドされる。`ergo_bind` からは両方が PUBLIC link されて transitive に取り込まれる |

LUDIARS パッケージ (モジュールではなく、モジュールが使う外部の LUDIARS 製パッケージ):

| 名前 | 概要 |
|---|---|
| `ergo-packages.json` | Pictor / Tela (link)・Lapilli (tool)・MUSA 系 / Curare (observe) の版と、どのモジュールがどれを要るか (`modules` 節) の宣言。取り込み (6 経路)・検査・記録は `cmake/packages/`、パッケージごとの結線は `cmake/packages/{pictor,tela}.cmake`。形式は `spec/data/ergo-packages.md`、ホスト手順は `spec/setup/packages.md` |
| ホストの宣言 (ホスト側 `ergo-packages.json` の `host` 節) | ホストが乗る Ergo の pin・使うパッケージ・非公開パッケージの宣言。`ergo_pkg init` がブートストラップ (`cmake/ergo-bootstrap.cmake`、ひな形は `cmake/packages/ergo-bootstrap.cmake.in`) と一緒に作る。事前取得は `cmake/packages/ErgoPackagePrefetch.cmake`。設計は `spec/feature/package-manager.md` |

## 状態メモ

- **`ergo_shuriken_migrator` は一覧に載せない**: Unity の ParticleSystem (Shuriken)
  を Ergo のパーティクル設定に変換する一回きりの移行用ライブラリと CLI で、
  使っているタイトルのツール側へ移してから Ergo から削除する予定
  (`spec/architecture/domains-and-ux.md` §5 の L1)。それまでは `legacy` ドメインに置く。
  `module_list.yaml` では `excluded` に理由付きで載せている。
- **仕様書の置き場所**: 初期からあるモジュールは `spec/feature/module/<名>.md`、
  2026-10 以降に足したモジュールは `spec/module/<名>.md` にある。一覧の仕様書の列は実物を指す。
- **`ergo_inspector` は廃止済み (2026-04-21)**: 機能調査の結果 `ergo_bind` の
  完全なサブセットと判定され、モジュール自体を廃止。`ergo_bind` に完全吸収
  された (詳細は `spec/feature/module/bind.md` の「歴史的経緯」節 / `spec/feature/tool/ergo.md`
  の「廃止プラン」節)。live tuning の唯一の窓口は `ergo_bind`。
- **`ergo_actor`**: `ergo_bind` に実装を委譲するシーングラフ基底。
  `ERGO_BUILD_ACTOR AND ERGO_BUILD_BIND` の両方が ON のときだけビルドされる
  (`ERGO_BUILD_BIND=OFF` なら STATUS 行を出して作らない。`ergo_cast` も同様)。
  逆に `ergo_physics2d` (→ `ergo_math`)・`ergo_ui_kit` / `ergo_ui_layout` /
  `ergo_resource` (→ `ergo_io`) は依存先が OFF だと構成エラーになる
  (一覧は [spec/setup/config-reference.md](spec/setup/config-reference.md) の「前提」列)。
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
単独パッケージではなく `tools/ergo/src/plugins/<id>/` に追加する。
組み込みプラグインの一覧 (id・対応する C++ モジュール・エンジン側の供給元・
仕様書) は [spec/feature/tool/ergo.md](spec/feature/tool/ergo.md) の
「プラグイン一覧」が正本で、登録の実体は `tools/ergo/src/core/registry.ts`。
