# Ergo

[![CI](https://github.com/LUDIARS/Ergo/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/LUDIARS/Ergo/actions/workflows/ci.yml)

LUDIARS / AdventureCube などのアプリで共通利用する C++ モジュール群と、
それらに付随する開発ツール群を集約するリポジトリ。モジュールは C++17 が基準で、
`ergo_math` / `ergo_physics2d` と Tela / Pictor のアダプタ (`ergo_tela_graph_tela` /
`ergo_render_viz_pictor`) だけが C++20。どちらの標準もリンクした利用側へ
`target_compile_features` で伝わる。

## セットアップ

設定・ビルド手順は用途別に [`spec/setup/`](spec/setup/) にまとめてある:

- **まず一周する**: [クイックスタート](spec/setup/quickstart.md) (clone → 例 `examples/compose_loop` のビルド → ブラウザで値を変えて反映)
- [複数モジュールを自分のフレームループにつなぐ](spec/setup/integrate-modules.md) / [CI のレグと手元での再現](spec/setup/ci.md)
- [C++ モジュールをビルド](spec/setup/build-cpp.md) / [tools/ergo を起動](spec/setup/run-tools-ergo.md) / [外部プラグイン (ERGO_PLUGIN_DIR)](spec/setup/external-plugins.md) / [プラグインのデータルート](spec/setup/plugin-data-roots.md) / [ブランチ+PR 運用](spec/setup/branch-and-pr.md)
- [サードパーティ・ライブラリ管理 (curl)](spec/setup/third-party-deps.md)
- [LUDIARS パッケージ・ハブ (Pictor / Tela の版固定・取り込み・照合、`ergo-packages.json`)](spec/setup/packages.md)
- [Visual Studio / Xcode プロジェクト生成 (依存ライブラリのリンクを含む)](spec/setup/ide-projects.md)
- 全設定キー: [spec/setup/config-reference.md](spec/setup/config-reference.md)

## 運用方針

**main ブランチに全モジュールを集約する。** 旧運用 (モジュールごとに `module/<名>`
ブランチで開発) は廃止した。理由:

- モジュール数 + 付随ツール数の増加で全容把握が難しくなった
- 複数ブランチをまたいだ変更 (例: モジュール ↔ ツールの schema 整合) が
  分割コミットで散らかっていた
- ホストアプリ側 (worktree 経由取り込み) も枝分かれの追跡コストが大きかった

main 一本に集約することで、横断変更を 1 コミットで行え、検索・レビュー・
リリース管理が単純になる。

## ディレクトリ構成

```
ergo/
├── README.md              # ← このファイル
├── CLAUDE.md              # AI/自動化エージェント向けルール
├── CMakeLists.txt         # 全モジュールをまとめてビルドするトップレベル
├── module_list.md         # モジュール一覧 (人間用)
├── module_list.yaml       # モジュール一覧 (機械処理用)
├── ergo-packages.json     # LUDIARS パッケージ (Pictor / Tela / Lapilli ...) の版と宣言
├── CMakePresets.json      # CI と同じ構成のプリセット (ci-linux-gcc / ci-linux-clang / ci-asan / ci-packages)
├── cmake/                 # モジュール前提の検査・ターゲットの仕上げ (Ergo:: ALIAS 等)・外部依存 (deps/)
├── cmake/packages/        # パッケージ・ハブ (マニフェスト・検査規則・6 経路の取り込み・記録、pictor/tela の結線、ブートストラップのひな形・事前取得)
├── spec/
│   ├── architecture/      # ドメインと UX の設計
│   ├── setup/             # 用途別セットアップガイドと全設定キー
│   ├── feature/module/    # モジュールの定義書 (初期からあるもの)
│   ├── feature/tool/      # Web ツール / プラグインの仕様書
│   ├── module/            # モジュールの定義書 (2026-10 以降に足したもの)
│   ├── domains/           # Anatomia のドメイン定義 (正本)
│   └── data/              # ファイル形式 (ergo-packages.json など)
├── include/ergo/<name>/   # 各モジュールの公開ヘッダ
├── src/<name>/            # 各モジュールの実装
├── tests/<name>/          # 各モジュールのテスト (tests/ 直下は共通の main と一覧の検査)
├── examples/              # モジュールを組み合わせるヘッドレスの使用例 (ERGO_BUILD_EXAMPLES)
├── tools/
│   ├── ergo/              # 統合 Web 開発者ツール (単一 Node サーバ + プラグイン)
│   ├── package_cli/       # ergo_pkg CLI
│   ├── package_viz/       # ergo_package_viz CLI
│   ├── render_viz/        # ergo_render_viz CLI
│   └── shuriken_migrator/ # (legacy) Unity Shuriken からの移行 CLI
├── unity/                 # Unity パッケージ (com.ludiars.ergo.packages)
├── shaders/               # パーティクル (compute / 描画) と描画ステージのシェーダ
├── lib/lapilli/           # submodule: 共有 CLI 起動ライブラリ (tools/ergo が使う)
├── benchmarks/            # opt-in マイクロベンチ (ERGO_BUILD_BENCHMARKS)
└── third_party/           # 同梱依存 (mini-gtest / nanosvg / earcut) と管理対象の外部依存の登録
```

## モジュール

38 モジュールと共通ライブラリ `ergo_common` を、所属する Anatomia ドメイン
([spec/domains/](spec/domains/README.md)) ごとに並べる。正本は
[module_list.md](module_list.md) (人間用: ヘッダ・実装・仕様書の場所) と
[module_list.yaml](module_list.yaml) (機械処理用: 依存・付随ターゲット・プラグイン id) で、
ここはその概観。仕様書は module_list.md の「仕様書」列が指す
(`spec/feature/module/<名>.md` か `spec/module/<名>.md`)。「ツール」列の id は
`tools/ergo` のプラグイン (一覧と接続方法は
[spec/feature/tool/ergo.md](spec/feature/tool/ergo.md) の「プラグイン一覧」)。

### foundation (共通土台)

| 名前 | 概要 | ツール |
|---|---|---|
| `ergo_common` | 共通ライブラリ (モジュールではなく常にビルド)。最小 JSON コーデック。別ターゲット `ergo_common_ws` は共有 WebSocket クライアント | — |
| `ergo_math` | C++20 concepts ベースの汎用数学 (Vec/Mat/SoA バッチ/Arena/ObjectPool) | — |

### input

| 名前 | 概要 | ツール |
|---|---|---|
| `ergo_input` | マウス・キーボード・ゲームパッド・USB HID 統一入力 (inject API でテスト可能) | — |

### observability (観測と調整)

| 名前 | 概要 | ツール |
|---|---|---|
| `ergo_log` | 4-level ロガー (Error/Warn/Info/Debug)、フレーム番号を行頭に埋め込む。出力先はシンクで差し替え | `log` (任意ターゲット `ergo_log_live` が送る) |
| `ergo_frame` | アプリ起動からの累計フレーム数 + rolling FPS + HUD 文字列 | — |
| `ergo_profile` | 性能タイムライン (マーカー注入 → Chrome Trace) | `profile` (トレースファイルを読む) |
| `ergo_bind` | 任意ホスト変数を WS で外部エディタに公開 (`BIND_VAR`) | `variable` |
| `ergo_custos` | 遠隔テストランナー Custos と話す in-process HTTP ブリッジ (`/health` `/screenshot` `/key`) | (外部: `LUDIARS/Custos`) |

### actor-character

| 名前 | 概要 | ツール |
|---|---|---|
| `ergo_actor` | シーングラフ基底 (ツリー + Actor 配下に変数公開, `ergo_bind` に委譲) | `variable` (で表示) |
| `ergo_character` | TCC 流 Character Controller (Brain/Check/Control/Effect 合成 + 優先度切替)。エンジン非依存 | — |

### scene-composition

| 名前 | 概要 | ツール |
|---|---|---|
| `ergo_scene` | 保存できるルックデヴ用シーン文書 (`*.scene.json` の CRUD・パッチ適用・カメラ行列) | `scene_editor` (任意ターゲット `ergo_scene_link` がつなぐ) |
| `ergo_cast` | Actor を名前付き Scene に束ねる軽量グルーピング層。`ergo_scene` とは別物 | — |

### gameplay-rules

| 名前 | 概要 | ツール |
|---|---|---|
| `ergo_health` | HP コンテナ + ダメージ/回復/死亡コールバック (game-lexicon: `health-system`) | — |
| `ergo_score` | スコアカウンタ + コンボ倍率 + ハイスコア通知 (game-lexicon: `score-system`) | — |
| `ergo_combo_counter` | 連続成功カウンタ + フルコンボ通知 (game-lexicon: `combo-counter`) | — |
| `ergo_timing_judge` | 音ゲー用 ms タイミング判定 PERFECT/GREAT/GOOD/MISS (game-lexicon: `timing-judge`) | — |
| `ergo_world_time` | グローバル time-scale コンポーザ (HitStop / HitSlow + observer) | — |
| `ergo_blackboard` | グローバル名前付き Property レジストリ (subscribe + カテゴリ lifecycle、名前で `find` / `subscribe` / `visit`) | — |

### physics

| 名前 | 概要 | ツール |
|---|---|---|
| `ergo_physics2d` | 2D 剛体物理エンジン (Box2D ライク、`ergo_math` を使うので C++20) | — |

### audio

| 名前 | 概要 | ツール |
|---|---|---|
| **`ergo_sound`** ⭐ | **コア音響エンジン** (自前 WAV デコーダ + ストリーミング + ミキサ + DSP + BPM クォンタイズ)。WAV のみで、圧縮フォーマットは未対応。rendering と並ぶ主要実装 | — |
| `ergo_audio` | ゲーム SE/BGM ファサード (FMOD Core 既定 + Dummy 自動フォールバック)。`ergo_sound` とは独立・横並び | — |

### rendering

| 名前 | 概要 | ツール |
|---|---|---|
| `ergo_render` | Pictor 横断オーケストレーション層 (`RenderContext` / `FrameComposer` / `ScreenshotBridge` / `StageRenderer` など) | — |
| `ergo_particle` | CPU パーティクル sim + Pictor Vulkan ビルボード描画 | `particle` / `vfx-suggest` / `vfx-breed` (任意ターゲット `ergo_particle_link` が設定を受け取る) |
| `ergo_gpu_particle` | Shuriken 相当 GPU コンピュートパーティクル (`IGpuBackend` 抽象) | — |

Ergo のモジュールに対応しないプラグインもある: `render_pipeline` / `pictor_perf` /
`visus` は Pictor の資産と計測を、`rive` は Rive ファイル (`.riv`) の再生を扱う
(同じく spec/feature/tool/ergo.md の「プラグイン一覧」)。

### ui-layout

| 名前 | 概要 | ツール |
|---|---|---|
| `ergo_ui` | SVG ラスタ + 9-slice フレームコンポーザ (RGBA8, Pictor/Vulkan 依存なし) | — |
| `ergo_ui_kit` | リテインドモードの UI コンポーネントフレームワーク (uidoc.json → レイアウト → 描画リスト) | — |
| `ergo_ui_layout` | データ駆動 UI レイアウト (JSON モデル + バインド + レイアウト解決 + 描画アダプタ) | `ui_layout` |
| `ergo_vector` | SVG path の三角形分割 + 押し出しメッシュ + 頂点アニメ | — |

### resource-io

| 名前 | 概要 | ツール |
|---|---|---|
| `ergo_io` | 最小ファイル I/O ラッパー (`<filesystem>` の薄膜, UTF-8 `std::string`) | — |
| `ergo_resource` | タグ駆動リソース自動選択 + 読み込み (JSON マニフェスト + Clio 同重みスコアラ)。MUSA Clio の C++ 移植 | — |
| `ergo_http` | libcurl バックエンドの同期 HTTP クライアント (`IHttpClient`/`get`/`post`)。既定 OFF (opt-in、`ERGO_WITH_CURL` を強制) | — |

### structure-visualization (構成の可視化)

| 名前 | 概要 | ツール |
|---|---|---|
| `ergo_tela_graph` | Ergo 可視化の共通出口。 配置済み関係図 → Tela `TELA_GRAPH 1` (検査付き) / `tela::Graph` | (外部: Tela `tela_view --graph`) |
| `ergo_graph_layout` | 汎用の層別配置 (抽象グラフ → 配置済み関係図、 決定的) | — |
| `ergo_render_viz` | Pictor 描画パイプライン (パス・アタッチメント・ポストプロセス・CPU 段・GPU 時間) の Tela 可視化 | CLI `ergo_render_viz` (pictor があるとき) |
| `ergo_package_model` | resolved.json / CMake File API codemodel v2 のリーダ (帰属・層・PKG-17 宣言漏れ)、 P4 の入力 (watch の報告・status.json) のリーダ | — |
| `ergo_package_scan` | 観測対象の兄弟チェックアウトから根拠付きの使用関係を読む (package.json / .gitmodules / gitlink / ported_from) | — |
| `ergo_package_viz` | パッケージ版一覧 / ターゲットのリンク図 / 使用関係図 / ホスト状態図 (P4) の Tela 可視化 (簡約・畳み込み・分割) | CLI `ergo_package_viz`、 ターゲット `ergo_package_graphs` |

### build-packages (ビルドとパッケージ取り込み)

| 名前 | 概要 | ツール |
|---|---|---|
| `ergo_package_manager` | パッケージマネージャ: ホストの宣言 (`host` 節) の読み取り・検査・漏れ検査・ブートストラップ・書式を保つ書き換え (add / remove / update)・ホストの状態 (status.json) | CLI `ergo_pkg` (`init` / `list` / `doctor` / `fetch` / `add` / `remove` / `update` / `status`) |
| `ergo_package_watch` | パッケージの更新の検知: tag / track / off-main の判定・run / review / merged と由来 (Revisor / Concordia)・前回状態と重複の抑止・報告と出来事・UPM ホスト (Unity プロジェクト) の読み手 | CLI `ergo_pkg` (`outdated` / `watch`)、 Unity パッケージ `unity/com.ludiars.ergo.packages` |

`ergo_sound` は Ergo のコア柱として外部ミドルウェアに依存せず発展させる
([spec/feature/module/sound.md](spec/feature/module/sound.md) 「位置付け」節)。
`ergo_shuriken_migrator` (legacy) は移行後に削除する予定のため一覧に載せない
(理由は module_list.md の「状態メモ」)。

## ビルド

トップレベル CMake が全モジュールを `add_library(ergo_<name>)` として公開する。
ホストアプリは `add_subdirectory(<ergo>)` するか、必要なモジュールだけを
ピックして取り込めばよい。

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

各モジュールは `ERGO_BUILD_<名>` オプションで個別に ON/OFF できる
(例 `-DERGO_BUILD_AUDIO=OFF`)。`ERGO_BUILD_TESTS` / `ERGO_BUILD_EXAMPLES` /
`ERGO_BUILD_DUMMY` / `ERGO_BUILD_BENCHMARKS` などもトップレベルオプション。
全オプションの既定・前提・前提が OFF のときの扱いは
[spec/setup/config-reference.md](spec/setup/config-reference.md) にまとめてある。

## ホストアプリからの利用

旧 worktree パターン (`git worktree add external/ergo/<mod> module/<mod>`) は
不要。AdventureCube のような利用側はリポジトリ全体を `external/ergo/` に
clone (or worktree) して、CMake で `add_subdirectory` するだけでよい。
ターゲットは `Ergo::<名>` (`ergo_<名>` の ALIAS) で指定するのを推奨する。

```cmake
add_subdirectory(external/ergo)
target_link_libraries(myapp PRIVATE Ergo::input Ergo::bind)
```

サブプロジェクトとして取り込んだ Ergo の既定 (詳細は
[spec/setup/build-cpp.md の「ホストアプリからの取り込み」](spec/setup/build-cpp.md#ホストアプリからの取り込み)):

- Ergo のテスト (`ERGO_BUILD_TESTS`) と使用例 (`ERGO_BUILD_EXAMPLES`) は OFF、
  `ERGO_EXCLUDE_FROM_ALL` は ON で、ホストがリンクしたライブラリだけがビルドされる。
  ホストのビルドで `ergo_pkg` CLI も作りたいときは
  `cmake --build <build> --target ergo_pkg_cli` を明示する (`all` には入らない)
- ライブラリは常に STATIC (`BUILD_SHARED_LIBS` は Ergo の中では無視) で PIC
- C++ 標準 (C++17、`ergo_math` / `ergo_physics2d` は C++20) と MSVC の `/utf-8` は
  リンクしたターゲットへ伝わる

ほかに: Visual Studio / Xcode のホストプロジェクト ([spec/setup/ide-projects.md](spec/setup/ide-projects.md))、
`ergo_pkg init` によるブートストラップと版の固定 ([spec/setup/packages.md](spec/setup/packages.md))、
複数モジュールをフレームループにつなぐ順序 ([spec/setup/integrate-modules.md](spec/setup/integrate-modules.md))。
以前に `ergo_pkg init` したホストは、Ergo を更新したらもう一度 `ergo_pkg init` を
実行する (ブートストラップのひな形が変わったため。そのままだと `ergo_pkg doctor` が
ブートストラップを Stale と報告する)。

## デモ集 (Pinacotheca)

Pictor のシェーダー / パイプラインプリセットのデモは [LUDIARS/Pinacotheca](https://github.com/LUDIARS/Pinacotheca) (ラテン語で「絵画館」) にある。
各デモは `ergo_bind` で値を公開しており、`tools/ergo` の variable plugin から実行中に調整できる
(手順は Pinacotheca の README と [tools/ergo を起動](spec/setup/run-tools-ergo.md))。
Pinacotheca が Ergo / Pictor を取り込む一方向で、Ergo から Pinacotheca は参照しない。
個別に作ったゲームは Pinacotheca ではなく、それぞれのゲームのリポジトリに置く。
