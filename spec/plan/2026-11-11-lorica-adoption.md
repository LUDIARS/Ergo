---
title: Lorica の全面適用と入力経路の上限 (job cpp-lorica-20261111)
type: plan
status: done
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

環境: Linux x86_64、GCC 13.3 / Clang 18.1.3、CMake 3.28.3、Ninja。Lorica は `-DLORICA_SOURCE_DIR=/home/user/Lorica`
(同じ commit)。固定 commit の取得 (GitHub) も一度通った。

| 構成 | 結果 |
|---|---|
| 着手時の先頭 `06361bb` (Lorica 無し)、GCC Release | `visus_plugin.cpp` が `std::nan` で通らず (`<cmath>` の入れ忘れ)。ctest 111/112 (`test_studio_plugins` は Not Run) |
| Lorica の適用のコミット、GCC Release | ビルド成功、ctest 112/112 |
| 最終、GCC Release (`ERGO_BUILD_BENCHMARKS=ON`) | ビルド成功、ctest 118/118 (この job で 6 本追加) |
| 最終、GCC ASan + UBSan (RelWithDebInfo `-g1`) | ctest 118/118 |
| clang 18 ASan + UBSan + `ERGO_BUILD_FUZZERS` | ctest 117/118 → `tests/studio/test_studio_log.cpp` の一時オブジェクトの参照 2 か所を直した後、Studio のテスト 4/4 |
| clang 18 TSan (Debug) | ctest 115/118 → `Server::stop()` と `accept_loop` の data race を直し、`test_studio_net` / `test_studio_plugins` は 3 回繰り返し通過。`ergo_sound` の `AudioEngine::setOutputCallback` と音声スレッドの data race (着手前から) は残り |
| fuzz (clang、各 30 秒) | `studio_net` 842,992 回・`json_min` 797,419 回・`custos_request_head` 2,062,286 回・`vector_svg` 561,789 回・`shuriken_prefab` 84,144 回、いずれもクラッシュ無し |
| `compile_commands.json` | Ergo の 429 翻訳単位すべてに `-fstack-protector-strong`、C の 1 本以外すべてに `unsafe_api.hpp`。打ち消す設定 (`-fno-stack-protector`、`-D_FORTIFY_SOURCE=0`、`execstack`、`-Wno-deprecated-declarations`) は無し |
| `verify_binary.py` | `ergo_studio`・`ergo_pkg`・`ergo_package_viz`・`shuriken_migrator`・例・ベンチ・テストと fuzz の回帰 90 本: PIE / NX / full RELRO あり |
| ホストに取り込まれたとき | ホストが先に Lorica を持つ / 持たない の 2 通りで構成・ビルド・実行でき、Ergo の 52 ターゲットに適用。ホストのターゲットには付かない |
| オプションの組み合わせ (構成のみ) | `ERGO_CXX_BASELINE=ON`・`ERGO_BUILD_DUMMY=ON` (157)・`ERGO_HARDENING=required` + `ERGO_HARDENING_PROFILE=extended`・`baseline` (EXTENDED 0)・clang: 通る。不正な profile、`ERGO_FETCH_DEPENDENCIES=OFF` で取得元無し: 構成エラー (意図どおり) |

### 4.1 速度への影響 (目安)

機械は 7 担当で共有して混んでいた。`benchmarks/` の 2 本を 3 回ずつ、best の中央値:

| ベンチマーク | Lorica 無し | baseline | 差 |
|---|---:|---:|---:|
| `curve.bake(4 keys)` | 132.9 ns | 196.4 ns | +48% |
| `curve.evaluate(16 keys)` | 9.76 ns | 11.34 ns | +16% |
| `minmax.evaluate(constants)` | 4.86 ns | 5.13 ns | +6% |
| `particle_cpu.update(10000)` (1 個あたり) | 6.77 ns | 7.40 ns | +9% |

測ったのは baseline (EXTENDED は描画・シミュレーションのライブラリには付けていない)。`_GLIBCXX_ASSERTIONS` と
stack protector が主な候補だが、内訳は分けて測っていない。

## 5. 追加: CI の Lorica の取得と「Lorica が掛かっているか」のテスト (2026-11-11、COMMON-4)

- CI: `ci.yml` の `build-and-test` (windows-msvc・linux-clang・linux-asan) と `packages-integration.yml` の `ci-packages` が、`third_party/dependencies.cmake` の固定 SHA を読み、secret `LUDIARS_REPO_TOKEN` で `LUDIARS/Lorica` を `build/_lorica` に取得 (`persist-credentials: false`。secret が空なら `::error::` で失敗) し、`-DLORICA_SOURCE_DIR=… -DERGO_HARDENING=required` で構成する。CMake 3.20 のレグと macOS は取得しない (守りなし、テストは skip)。(§8: main がワークフローを削除したので、この取得のステップも無くなった。)
- テスト 6 本 (`cmake/ErgoHardeningChecks.cmake`、`tests/hardening/`): `test_lorica_applied` / `test_lorica_applied_extended` (翻訳単位の中から)、`test_lorica_compile_commands` (`compile_commands.json` の 431 単位)、`test_lorica_verify_binary_{ergo_studio,ergo_pkg_cli,test_lorica_applied}` (python3 があるときだけ)。手順は [../setup/hardening.md](../setup/hardening.md)。
- わざと壊した確認: `test_json_min` に `-fno-stack-protector`、`test_lorica_applied` に `-U_GLIBCXX_ASSERTIONS` を足す → `test_lorica_compile_commands` が 2 単位を名指しして失敗、`test_lorica_applied` も失敗 (元に戻して全通過)。強制 include を外して手でコンパイルした probe は `UnsafeApiHeaderForceIncluded` で失敗。
- 結果: GCC Release (`ERGO_BUILD_BENCHMARKS=ON`) で ctest 124/124 (118 + 6)。Windows・clang・ASan のレグとワークフローの実行はこの環境では未確認。

## 6. 追加: main (#95 授業連携プラグイン lesson) の取り込み (2026-11-11)

- `origin/main` (`f0219fd`) を merge。衝突 2 か所: `CMakeLists.txt` の `ergo_studio` (main の `ergo_studio_views` のリンクと `ergo_hardening_mark_network(ergo_studio)` の両方を残す)、`spec/setup/third-party-deps.md` の依存表 (main の curl の行と lorica の行の両方、main の curl の説明文も残す)。
- main で増えたターゲット 8 本 (+ この job の `test_studio_lesson_limits`) も `ergo_harden_targets()` を通る: 158 ターゲット (EXTENDED 8)、未適用 0、`test_lorica_compile_commands` は 457 単位で通過。unsafe API の拒否で落ちる呼び出しは無かった。lesson のコードは `ergo_studio_plugins` (EXTENDED)、実行ファイルは `ergo_studio` (EXTENDED) に入る。`ergo_studio_views` (UI) と例 `lesson_action_feel` (ergo_bind で手元の Studio に繋ぐだけ) は baseline。
- 上限を入れた経路: odeum-lesson の応答 (`lesson_protocol`: 本文 1 MiB・値 16384・深さ 16・手順 256・features / demo.states 各 256・文字列 4096 バイト・保持する文字列の合計 256 KiB。それまでは JSON の既定 64 MiB / 値 1,048,576 だけ)、接続文字列 (`lesson_link`: 2048 バイト)、WinHTTP の応答本文 (64 MiB。libcurl 側は main で済み、WinHTTP は無制限だった)。境界テストは `tests/studio/test_studio_lesson_limits.cpp` (7 件)。
- 既にあったもの (確認だけ): `/lesson/api/*` (Studio の HTTP の既存の上限 + `check_mutation`)、libcurl の応答 64 MiB と要求の検査 (`http_request_check`)、`poll_ms` の 1〜10 s への丸め、トークン 32 桁・エラーコード 40 文字。`studio_settings` はファイルを読まない (環境変数と引数だけ、`PORT` は範囲検査済み) ので変更なし。
- 結果: GCC Release ctest 131/131。lesson まわり 5 本 (`test_studio_lesson`・`_game`・`_limits`・`test_studio_platform_http`・`test_studio_settings`) は clang TSan と GCC ASan + UBSan (RelWithDebInfo `-g1`) でそれぞれ 2 回ずつ通過。WinHTTP の変更は Windows でビルドしていない (未確認)。

## 7. 直し: 守りのプロファイルが使えない構成でも `Lorica::boundary` を入れる (2026-11-11)

- 不具合: CMake 3.24 未満 / macOS などで `ergo_hardening_setup()` が Lorica を取得せずに戻り、`<lorica/...>` を include する parser (json_min・ws_client・studio net など) がコンパイルで落ちていた (CI の `linux-gcc-cmake320`)。§2・§5 の「守りなしでビルドする」は実際にはビルドできなかった。
- 直し (`cmake/ErgoHardening.cmake`): その構成でも `Lorica::boundary` を取得して全ターゲットに `PRIVATE $<BUILD_INTERFACE:...>` で渡す。外すのは `lorica_harden_target` / `lorica_deprecate_unsafe_apis` だけ。CMake 3.24 以上は Lorica の `CMakeLists.txt` をそのまま取り込む (保護ライブラリ・テスト・fuzz が OFF なら OS で FATAL になる箇所は無い)。3.24 未満はソースだけ (`LORICA_SOURCE_DIR` → `ERGO_SOURCE_DIR_LORICA` → 固定 commit、`ergo_populate_dependency`) を取り、include の場所と `cxx_std_17` だけの INTERFACE `lorica_boundary` / `Lorica::boundary` を作る。ホストが持っていればそれを使う。取得元が無い・ヘッダが無いときは構成エラー。
- `test_lorica_applied*` は守りなしの構成でも `checked_add` / `input_budget` を実行してから skip。CI は macOS と `linux-gcc-cmake320` でも Lorica を取得する (`ERGO_HARDENING=required` は付けない)。
- 確認: CMake 3.20.5 (CI の `linux-gcc-cmake320` と同じ構成) でビルドと ctest 128/128 (skip 3: `test_lorica_applied`・`_extended`・`test_lorica_compile_commands`。`verify_binary` は登録しない)。CMake 3.24 以上で OS が対応外の枝 (macOS) は記述を読んだだけで未実行。

## 8. 追加: main (#97) の取り込みと Praesidium の登録 (2026-11-12)

- main が GitHub のワークフロー (`ci.yml`・`packages-integration.yml`) を削除し、マージのゲートを Revisor のローカルレビューにした。§5 の CI の取得のステップも消えた。Lorica を使う構成のコマンド (`-DLORICA_SOURCE_DIR`、守りのプロファイルが使える構成だけ `-DERGO_HARDENING=required`、CMake 3.24 未満の扱い) は `spec/setup/ci.md` のローカルの手順に入れた。
- `praesidium.json`: この PR が変えた翻訳単位 34 本のうち未登録の 27 本を main と同じ書き方で足し、Lorica のヘッダを include する 11 本 (既存 3 本を含む) に `"packages": ["lorica"]` を付けた。運用側の `praesidium.host.json` に `lorica` の対応付けが要る (`spec/setup/hardening.md`)。
- Praesidium の事前検査 (Revisor の `scanRepository` を手元で直接呼んだ。clang 18、`praesidium.host.json` に `lorica` = Lorica の include) は `failed`。最初は文字列の規則 `native.unbounded-copy` が `tests/hardening/test_lorica_applied.cpp` の probe (strcpy の名前を未評価の decltype で引くだけで呼ばない) を 2 か所拾ったので、Lorica のヘッダと同じく名前をマクロで綴るよう直した (再実行で 0 件)。native 解析は 34 本中 32 本が passed。失敗は `src/studio/platform/winhttp_client.cpp` (Windows 専用。Linux のホストでは `windows.h` が無い) と `src/ui/svg_raster.cpp` (第三者の `third_party/nanosvg/nanosvg.h` の `nsvg__xformInverse` が特異行列のとき `inv` ではなく `t` を単位行列にして返し、`inv` が未初期化のまま読まれる、と解析器が報告。第三者のコードなので直していない。人間の判断が要る)。
- 結果: CMake 3.28 + `ERGO_HARDENING=required` で ctest 131/131。
