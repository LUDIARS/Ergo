# Ergo

LUDIARS / AdventureCube などのアプリで共通利用する C++17 モジュール群と、
それらに付随する開発ツール群を集約するリポジトリ。

## セットアップ

設定・ビルド手順は用途別に [`spec/setup/`](spec/setup/) にまとめてある:

- [C++ モジュールをビルド](spec/setup/build-cpp.md) / [tools/ergo を起動](spec/setup/run-tools-ergo.md) / [外部プラグイン (ERGO_PLUGIN_DIR)](spec/setup/external-plugins.md) / [プラグインのデータルート](spec/setup/plugin-data-roots.md) / [ブランチ+PR 運用](spec/setup/branch-and-pr.md)
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
├── spec/
│   ├── module/<name>.md   # 各モジュールの定義書
│   └── tool/<name>.md     # Web ツール / プラグインの仕様書
├── include/ergo/<name>/   # 各モジュールの公開ヘッダ
├── src/<name>/            # 各モジュールの実装
├── tests/<name>/          # 各モジュールのテスト
├── tools/ergo/            # 統合 Web 開発者ツール (単一 Node サーバ + プラグイン)
├── benchmarks/            # opt-in マイクロベンチ (ERGO_BUILD_BENCHMARKS)
└── third_party/           # 同梱依存 (mini-gtest / nanosvg / stb)
```

## モジュール

| 名前 | 概要 | ヘッダ | tools |
|---|---|---|---|
| `ergo_input`     | マウス・キーボード・ゲームパッド・USB HID 統一入力       | `include/ergo/input/`     | — |
| `ergo_particle`      | CPU パーティクル sim + Pictor Vulkan ビルボード描画            | `include/ergo/particle/`     | `tools/ergo/` (`particle` plugin) |
| `ergo_gpu_particle`  | Shuriken相当 GPU コンピュートパーティクル (`IGpuBackend` 抽象) | `include/ergo/gpu_particle/` | — |
| `ergo_bind`          | 任意ホスト変数を WS で外部エディタに公開 (`BIND_VAR`)          | `include/ergo/bind/`         | `tools/ergo/` (`variable` plugin) |
| `ergo_actor`         | シーングラフ基底 (ツリー + Actor 配下に変数公開, ergo_bind に委譲) | `include/ergo/actor/`        | (variable プラグインで表示) |
| `ergo_frame`         | アプリ起動からの累計フレーム数 + rolling FPS + HUD 文字列      | `include/ergo/frame/`        | — |
| `ergo_log`           | 4-level ロガー (Error/Warn/Info/Debug)、フレーム番号を行頭に埋め込む | `include/ergo/log/`          | — |
| `ergo_io`            | 最小ファイル I/O ラッパー (`<filesystem>` の薄膜, UTF-8 `std::string`) | `include/ergo/io/`           | — |
| **`ergo_sound`** ⭐  | **コア音響エンジン** (自前 WAV/OGG デコーダ + ミキサ + DSP + BPM クォンタイズ)。rendering と並ぶ主要実装 | `include/ergo/sound/`        | — |
| `ergo_audio`         | ゲーム SE ファサード (FMOD Core 既定 + Dummy 自動フォールバック)。`ergo_sound` とは独立・横並び | `include/ergo/audio/`        | — |
| `ergo_world_time`    | グローバル time-scale コンポーザ (HitStop / HitSlow + observer)。Foundation/WorldTimeScale の C++ ポート | `include/ergo/world_time/`   | — |
| `ergo_blackboard`    | グローバル名前付き Property レジストリ (subscribe + カテゴリ lifecycle)。Foundation/Blackboard の C++ ポート | `include/ergo/blackboard/`   | — |
| `ergo_ui`            | SVG ラスタ + 9-slice フレームコンポーザ (RGBA8, Pictor/Vulkan 依存なし)        | `include/ergo/ui/`           | — |
| `ergo_custos`        | 遠隔テストランナー Custos と話す in-process HTTP ブリッジ (`/health` `/screenshot` `/key`) | `include/ergo/custos/`       | (外部: `LUDIARS/Custos`) |
| `ergo_health`        | HP コンテナ + ダメージ/回復/死亡コールバック (game-lexicon: `health-system`)   | `include/ergo/health/`       | — |
| `ergo_score`         | スコアカウンタ + コンボ倍率 + ハイスコア通知 (game-lexicon: `score-system`)    | `include/ergo/score/`        | — |
| `ergo_combo_counter` | 連続成功カウンタ + フルコンボ通知 (game-lexicon: `combo-counter`)              | `include/ergo/combo_counter/`| — |
| `ergo_timing_judge`  | 音ゲー用 ms タイミング判定 PERFECT/GREAT/GOOD/MISS (game-lexicon: `timing-judge`) | `include/ergo/timing_judge/` | — |
| `ergo_character`     | TCC 流 Character Controller (Brain/Check/Control/Effect 合成 + 優先度切替)。エンジン非依存 | `include/ergo/character/`    | — |

詳細は `spec/module/<名>.md` 参照。`ergo_sound` は Ergo のコア柱として
外部ミドルウェアに依存せず発展させる (`spec/module/sound.md` 「位置付け」節)。

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
(例 `-DERGO_BUILD_AUDIO=OFF`)。`ERGO_BUILD_TESTS` / `ERGO_BUILD_DUMMY` /
`ERGO_BUILD_BENCHMARKS` もトップレベルオプション。

## ホストアプリからの利用

旧 worktree パターン (`git worktree add external/ergo/<mod> module/<mod>`) は
不要。AdventureCube のような利用側はリポジトリ全体を `external/ergo/` に
clone (or worktree) して、CMake で `add_subdirectory` するだけでよい。
