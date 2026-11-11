---
title: Lorica の全面適用と入力経路の上限 (job cpp-lorica-20261111)
type: plan
status: in_progress
service: ergo
domain: build-packages
updated: 2026-11-11
---

# Lorica の全面適用と入力経路の上限

- branch: `claude/cpp-lorica-handoff-3yj88x` (着手時の先頭 `06361bb`)
- Lorica: `LUDIARS/Lorica` の `5e8281b0e60fa16f814e16eaa620715937e8618d` (2026-11-11 の main)
- 使い方の正本: [../setup/hardening.md](../setup/hardening.md)

## 1. 棚卸し

### 1.1 ビルド単位

Linux (GCC 13.3、Ninja、Release、`ERGO_AUDIO_BACKEND=dummy`、`ERGO_BUILD_BENCHMARKS=ON`) の着手時の構成で
コンパイルされるもの: **141** (ライブラリ 48・C の MODULE 1・実行ファイル 92)。この job で回帰の実行ファイル
6 本 (`test_request_head`、`fuzz_regress_*` 5 本) を足し、**147**。

| 種類 | ターゲット | 作る条件 | Linux |
|---|---|---|---|
| モジュールのライブラリ (STATIC) | `ergo_common` `ergo_common_ws` `ergo_input` `ergo_particle` `ergo_particle_link` `ergo_bind` `ergo_actor` `ergo_cast` `ergo_scene` `ergo_scene_link` `ergo_sound` `ergo_gpu_particle` `ergo_frame` `ergo_profile` `ergo_log` `ergo_log_live` `ergo_io` `ergo_audio` `ergo_world_time` `ergo_blackboard` `ergo_ui` `ergo_custos` `ergo_math` `ergo_physics2d` `ergo_health` `ergo_resource` `ergo_score` `ergo_combo_counter` `ergo_timing_judge` `ergo_character` `ergo_ui_kit` `ergo_studio_net` `ergo_studio_host` `ergo_studio_ui` `ergo_studio_platform` `ergo_studio_plugins` `ergo_shuriken_migrator` `ergo_render` `ergo_tela_graph` `ergo_render_viz` `ergo_graph_layout` `ergo_package_model` `ergo_package_scan` `ergo_package_viz` `ergo_package_manager` `ergo_package_watch` `ergo_ui_layout` `ergo_vector` | 各 `ERGO_BUILD_<名>` (既定 ON) | ビルドできる |
| テストの土台 | `ergo_gtest_main` | `ERGO_BUILD_TESTS` | ビルドできる |
| C の plugin pack (MODULE) | `ergo_studio_echo_pack` | `ERGO_BUILD_TESTS` | ビルドできる |
| テスト | `test_*` / `gpu_test_particle_system` (85) | `ERGO_BUILD_TESTS` | ビルドできる |
| ツール | `ergo_studio` (headless server)、`ergo_pkg_cli`、`ergo_package_viz_cli`、`shuriken_migrator` | 各モジュール | ビルドできる |
| 例・ベンチ | `ergo_example_compose_loop`、`bench_particle_cpu`、`bench_curve` | `ERGO_BUILD_EXAMPLES` / `ERGO_BUILD_BENCHMARKS` | ビルドできる |
| dummy ライブラリ (12) | `ergo_{input,particle,bind,actor,sound,gpu_particle,frame,profile,log,io,world_time,blackboard}_dummy` | `ERGO_BUILD_DUMMY` (既定 OFF) | ビルドできる (§4 で確認) |
| HTTP クライアント | `ergo_http`、`test_http_client`、`test_thirdparty_deps` | `ERGO_BUILD_HTTP` / `ERGO_BUILD_THIRDPARTY_SMOKE` (curl を取得) | 取得が要る |
| Pictor / Tela / Vulkan | `ergo_studio_shell`、`test_studio_shell_text`、`ergo_render_viz_pictor`、`ergo_render_viz_cli`、`ergo_tela_graph_tela`、`test_tela_graph_adapter`、`test_package_viz_tela`、`ergo_particle` の描画 (同じターゲット)、`ergo_render` の実描画 (同じターゲット) | `pictor` / `Tela::Graph` / glfw / Vulkan があるとき | **この環境ではビルドできない** (Vulkan SDK・Pictor・Tela・glfw が無い) |
| Windows 専用 | `ergo_studio_platform` の WinHTTP / 資格情報 / プロセスの実装 (同じターゲット) | `WIN32` | **ビルドできない** |
| FMOD | `ergo_audio` の FMOD 実装 (同じターゲット) | FMOD SDK | **ビルドできない** |
| fuzz | `fuzz_{studio_net,json_min,custos_request_head,vector_svg,shuriken_prefab}` | `ERGO_BUILD_FUZZERS` (Clang、この job で追加) | ビルドできる |

### 1.2 第三者のコード

| もの | 形 | Lorica |
|---|---|---|
| nanoSVG (`third_party/nanosvg`) | ヘッダだけ (ergo_ui の 1 TU が実装を持つ) | ergo_ui の守りがそのまま掛かる |
| earcut (`third_party/earcut`) | ヘッダだけ | 使う側のターゲットの守り |
| mini-gtest (`third_party/gtest`) | ヘッダだけ | `ergo_gtest_main` の守り |
| curl (`cmake/deps/curl.cmake`) | ソースから (`ERGO_WITH_CURL`、既定 OFF) | **未適用** (§3) |
| Pictor / Tela (パッケージ・ハブ) | 別の LUDIARS リポのソース | 各リポの担当 (Ergo からは掛けない) |
| FMOD | ビルド済み | 対象外 |
| Lapilli (`lib/lapilli`、submodule) | `tools/ergo` (TypeScript) 用 | 対象外 |

### 1.3 外から入る入力の経路

| 経路 | 場所 | 着手時の上限 | この job |
|---|---|---|---|
| Studio の HTTP サーバ | `src/studio/net/http_parser.cpp` `server.cpp` | ヘッダ 64 KiB・本文 32 MiB。ヘッダ行の数・同時接続は無制限 | 済み (§2) |
| Studio の WebSocket | `src/studio/net/ws_frame.cpp` | 32 MiB | 済み (検査を `checked_*` に) |
| Custos ブリッジ (HTTP/1.0) | `src/custos/http_server.cpp` | ヘッダ 16 KiB。**本文は Content-Length を信じて無制限に受信**、`stoul` は `-1` を受け付けた | 済み |
| WS クライアント (`ergo_common_ws`) | `src/common/ws_client.cpp` | 1 フレーム 1 MiB | 済み (境界テスト + `checked_add`) |
| JSON (`json_min`、WS の全プロトコル・マニフェスト・シーン・レイアウト・パッケージの JSON が通る) | `src/common/json_min.cpp` | 深さ 64 のみ。値の数は無制限 (32 MiB の `[0,0,…]` で 1 GB 超)、`\uZZZZ` を受け付けた | 済み |
| SVG のラスタ化 (nanoSVG) | `src/ui/svg_raster.cpp` | 無し (`w*h*4` を確保、`int` への変換あふれ) | 済み |
| SVG のパス (`ergo_vector`) | `src/vector/svg_parser.cpp` | 無し。`std::regex` が長い属性でスタックを溢れさせ、`std::stof` が例外を投げた | 済み |
| Unity Shuriken の移行 | `src/shuriken_migrator/*.cpp` | ファイルを丸ごと読む、親子の復元が再帰 (深さ無制限) | 済み |
| WAV | `src/sound/audio_decoder.cpp` | チャンネル数・サンプルレートを信じる (65535 ch で 1 回の decode に 200 MB) | 済み |
| git の index (`ergo_package_scan`) | `src/package_scan/git_index_reader.cpp` | 範囲の検査あり (読んで確認、変更なし) | 既存のまま |
| UI レイアウト / シーン / リソースのマニフェスト | `src/ui_layout/json_codec.cpp` `src/scene/scene.cpp` `src/resource/catalog.cpp` `tools/studio/plugins/scene_editor/parse_scene.cpp` | JSON は上の `json_min` の上限が掛かる。ファイルの読み込み (`ergo_io`) に大きさの上限は無い | **残り** |
| Studio のプラグイン (Markdown・ログ・VFX 提案の LLM 出力・プラグインの取得) | `tools/studio/plugins/**` | 未確認 | **残り** |
| 子プロセスの出力 (`claude` CLI、git) | `tools/studio/plugins/common/claude_cli.cpp`、`tools/package_cli/process.cpp` | 未確認 | **残り** |
| 外部の plugin pack (DLL / .so の読み込み) | `src/studio/host/external_loader.cpp` | コードの読み込みなので入力の上限の対象外 (署名・置き場の問題) | 対象外 |
| 引数・環境変数 | 各 CLI、`ERGO_PLUGIN_DIR` など | 未確認 | **残り** |

### 1.4 対応 OS・ツールチェイン

| 環境 | Ergo の CI | Lorica |
|---|---|---|
| Windows MSVC (Visual Studio 2022) | 必須レグ | 対応 (baseline: `/GS /sdl /guard:cf`…。EXTENDED は MSVC 19.44 以上の x64) |
| Linux GCC (CMake 3.20.5 のレグを含む) | 必須 | 対応。ただし **CMake 3.24 以上が要る** ので、CMake 3.20 のレグでは守り無し (WARNING) |
| Linux clang | 必須 | 対応 (glibc 2.40 以上の Clang では危険な C 関数の拒否だけ断る) |
| Linux ASan + UBSan | 必須 | 対応 |
| macOS (AppleClang) | 任意 | **非対応** (WARNING で守り無し) |
| Android NDK / iOS | ホストのビルド | **非対応** (同上) |
| Emscripten / MinGW | ホストのビルド | **非対応** (同上) |

### 1.5 着手時の状態

着手時の先頭 (`06361bb`) を `git archive` で取り出して Linux GCC で構成・ビルド・`ctest`: 結果は
[§4](#4-確かめたこと) に書く。

## 2. やったこと

1. `cmake/ErgoHardening.cmake`: Lorica の取り込み (ホストのもの → `LORICA_SOURCE_DIR` → `ERGO_SOURCE_DIR_LORICA` →
   固定 commit の取得) と、全ターゲットへの `Lorica::boundary` / baseline / EXTENDED / 危険な C 関数の拒否、
   未適用で構成エラーにする検査。`third_party/dependencies.cmake` に固定 commit を登録。
2. 入力の上限 ([../setup/hardening.md](../setup/hardening.md) の表)。各経路に、上限ちょうど / +1 / 壊れた入力 /
   巨大な宣言の境界テスト。
3. `cmake/ErgoFuzz.cmake`: libFuzzer の入口 5 本 (`ERGO_BUILD_FUZZERS`) と、どのコンパイラでも走る回帰
   `fuzz_regress_*`。プリセット `ci-tsan` / `fuzz-clang`。
4. 守りで見つかったソースの直し: `tools/studio/plugins/visus/visus_plugin.cpp` の `<cmath>` の入れ忘れ。

## 3. 残り

- §1.3 の **残り** の経路 (ファイルの読み込みの大きさの上限、Studio のプラグイン、子プロセスの出力、引数・環境変数)。
- curl (`ERGO_WITH_CURL`) の Lorica の適用: curl は C のソースで、Ergo の歩き方は取得物の置き場を除くので
  未適用。`LoricaInject.cmake` か `ergo_harden_targets` の対象に `libcurl_static` を足すかは人間の判断。
- Windows MSVC・macOS・Pictor + Vulkan の構成での確認 (この環境では実行できない)。
- `praesidium.json`: Lorica のヘッダを include した翻訳単位の解析設定に Lorica の include の場所が無い。
- CI: ASan のレグは今のまま。TSan と fuzz はレグにしていない (`ci-tsan` / `fuzz-clang` は手元用)。

## 4. 確かめたこと

(報告 `/home/user/.cpp-deps/reports/Ergo-report.md` と同じ内容をここに残す。§4 は最終の結果で更新する。)
