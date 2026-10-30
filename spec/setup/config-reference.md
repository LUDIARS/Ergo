# 設定リファレンス (環境変数 / ビルドフラグ / ポート)

Ergo で実在する CMake オプション・環境変数・ポートの早見表。**CMake オプションの
一覧はこのページが正本** で、[build-cpp.md](build-cpp.md) などの用途別ガイドは
ビルドの形に関わるものだけを抜き出して説明する。出典ファイルを併記する。

ctest `test_module_catalog` (`tests/test_module_catalog.cmake`) は、
`CMakeLists.txt` / `third_party/dependencies.cmake` / `cmake/**/*.cmake` /
`cmake/packages/ergo-bootstrap.cmake.in` / `examples/CMakeLists.txt` で宣言された
`ERGO_*` のキャッシュ変数がすべてこのページに載っているかを検査する。

## CMake オプション

`-D<KEY>=<値>` で指定する (ホストからは `add_subdirectory(<ergo>)` より前に
キャッシュ変数として設定する)。宣言の正本は [`../../CMakeLists.txt`](../../CMakeLists.txt)
と、表の「出典」に挙げたファイル。

「トップレベル」は Ergo 自身を `cmake -S <ergo>` で構成した場合、「サブプロジェクト」は
ホストの `add_subdirectory` / FetchContent / `ergo_pkg` のブートストラップで取り込まれた
場合 (判定は `CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR`)。

「前提が満たされないとき」の意味:

- **構成エラー**: `FATAL_ERROR` で止まり、欠けている組み合わせをすべて 1 つのメッセージに並べる
  (`cmake/ErgoModuleRequirements.cmake`)。リンク時 (`cannot find -lergo_io` など) まで遅らせない
- **スキップ (STATUS)**: そのターゲットを作らず、`ergo_actor: requires ERGO_BUILD_BIND — skipped`
  のような STATUS 行を出す
- **宣言されない**: オプション自体が親モジュールのブロックの中で宣言されるので、親が OFF だと
  キャッシュにも現れない

| キー | 既定 | ビルドするもの / 効果 | 前提 | 前提が満たされないとき |
|---|---|---|---|---|
| **全体** | | | | |
| `ERGO_BUILD_TESTS` | トップレベル ON / サブプロジェクト OFF | モジュールテスト (CTest 登録) と `cmake -P` の検査テスト (`test_module_catalog` / `test_packages_*` / `test_host_*`) | — | — |
| `ERGO_BUILD_EXAMPLES` | トップレベル ON / サブプロジェクト OFF | `examples/` のヘッドレスな使用例 (`ergo_example_compose_loop`。テスト ON なら `example_compose_loop` / `example_compose_loop_trace` / `example_compose_loop_trace_json`) | `INPUT` / `CHARACTER` / `ACTOR` / `BIND` / `CAST` / `WORLD_TIME` / `HEALTH` / `COMBO_COUNTER` / `SCORE` / `TIMING_JUDGE` / `BLACKBOARD` / `LOG` / `FRAME` / `PROFILE` / `AUDIO` | スキップ (STATUS `skip example compose_loop: needs ...`) |
| `ERGO_EXCLUDE_FROM_ALL` | サブプロジェクトで ON / トップレベル OFF。`ERGO_BUILD_TESTS=ON` の間は効かない | Ergo のディレクトリに `EXCLUDE_FROM_ALL` を付ける。ホストの `all` にはホストがリンクしたものだけが入る (`ergo_pkg` CLI は `--target ergo_pkg_cli` で明示) | — | — |
| `ERGO_POSITION_INDEPENDENT_CODE` | ON | Ergo のスコープで `CMAKE_POSITION_INDEPENDENT_CODE` を設定する。静的ライブラリをホストの共有ライブラリ / プラグインにリンクできる | — | — |
| `ERGO_BUILD_DUMMY` | OFF | `ergo_<名>_dummy` (no-op 実装): `input` / `particle` / `bind` / `actor` / `sound` / `gpu_particle` / `frame` / `profile` / `log` / `io` / `world_time` / `blackboard` | 各モジュールの `ERGO_BUILD_<名>` | そのモジュールの dummy を作らない |
| `ERGO_BUILD_BENCHMARKS` | OFF | マイクロベンチ `bench_particle_cpu` / `bench_curve` (CTest には登録しない) | `GPU_PARTICLE` | 作らない (メッセージなし) |
| **モジュール (`ERGO_BUILD_<名>`)** | | | | |
| `ERGO_BUILD_INPUT` | ON | `ergo_input` | — | — |
| `ERGO_BUILD_PARTICLE` | ON | `ergo_particle` | — | — |
| `ERGO_BUILD_GPU_PARTICLE` | ON | `ergo_gpu_particle` | — | — |
| `ERGO_BUILD_BIND` | ON | `ergo_bind` (`ergo_common` / `ergo_common_ws` をリンク) | — | — |
| `ERGO_BUILD_ACTOR` | ON | `ergo_actor` | `BIND` | スキップ (STATUS) |
| `ERGO_BUILD_CAST` | ON | `ergo_cast` | `ACTOR` + `BIND` | スキップ (STATUS) |
| `ERGO_BUILD_SCENE` | ON | `ergo_scene` | — | — |
| `ERGO_BUILD_SOUND` | ON | `ergo_sound` | — | — |
| `ERGO_BUILD_FRAME` | ON | `ergo_frame` | — | — |
| `ERGO_BUILD_PROFILE` | ON | `ergo_profile` | — | — |
| `ERGO_BUILD_LOG` | ON | `ergo_log` | — | — |
| `ERGO_BUILD_IO` | ON | `ergo_io` | — | — |
| `ERGO_BUILD_AUDIO` | ON | `ergo_audio` (バックエンドは `ERGO_AUDIO_BACKEND`) | — | — |
| `ERGO_BUILD_WORLD_TIME` | ON | `ergo_world_time` | — | — |
| `ERGO_BUILD_BLACKBOARD` | ON | `ergo_blackboard` | — | — |
| `ERGO_BUILD_UI` | ON | `ergo_ui` | — | — |
| `ERGO_BUILD_CUSTOS` | ON | `ergo_custos` | — | — |
| `ERGO_BUILD_HEALTH` | ON | `ergo_health` | — | — |
| `ERGO_BUILD_SCORE` | ON | `ergo_score` | — | — |
| `ERGO_BUILD_COMBO_COUNTER` | ON | `ergo_combo_counter` | — | — |
| `ERGO_BUILD_TIMING_JUDGE` | ON | `ergo_timing_judge` | — | — |
| `ERGO_BUILD_CHARACTER` | ON | `ergo_character` | — | — |
| `ERGO_BUILD_MATH` | ON | `ergo_math` (**C++20**。リンクした側にも C++20 が伝わる) | — | — |
| `ERGO_BUILD_PHYSICS2D` | ON | `ergo_physics2d` (**C++20**) | `MATH` | 構成エラー |
| `ERGO_BUILD_RESOURCE` | ON | `ergo_resource` | `IO` | 構成エラー |
| `ERGO_BUILD_UI_KIT` | ON | `ergo_ui_kit` | `IO` | 構成エラー |
| `ERGO_BUILD_UI_LAYOUT` | ON | `ergo_ui_layout` | `IO` | 構成エラー |
| `ERGO_BUILD_VECTOR` | ON | `ergo_vector` | — | — |
| `ERGO_BUILD_RENDER` | ON | `ergo_render` (Vulkan 非依存部分は常に。実描画経路は `ERGO_RENDER_REQUIRE_REAL` の行) | — | — |
| `ERGO_BUILD_TELA_GRAPH` | ON | `ergo_tela_graph`。`Tela::Graph` ターゲットがあれば `ergo_tela_graph_tela` (C++20) も | — | — |
| `ERGO_BUILD_RENDER_VIZ` | ON | `ergo_render_viz`。`pictor` ターゲットがあれば `ergo_render_viz_pictor` (C++20) と CLI `ergo_render_viz` も | `TELA_GRAPH` | スキップ (STATUS) |
| `ERGO_BUILD_GRAPH_LAYOUT` | ON | `ergo_graph_layout` | `TELA_GRAPH` | スキップ (STATUS) |
| `ERGO_BUILD_PACKAGE_MODEL` | ON | `ergo_package_model` | — | — |
| `ERGO_BUILD_PACKAGE_SCAN` | ON | `ergo_package_scan` | — | — |
| `ERGO_BUILD_PACKAGE_VIZ` | ON | `ergo_package_viz`、CLI `ergo_package_viz`、ターゲット `ergo_package_graphs` | `PACKAGE_MODEL` + `PACKAGE_SCAN` + `GRAPH_LAYOUT` | スキップ (STATUS) |
| `ERGO_BUILD_PACKAGE_MANAGER` | ON | `ergo_package_manager` | — | — |
| `ERGO_BUILD_PACKAGE_WATCH` | ON | `ergo_package_watch` (`ergo_pkg outdated` / `watch` もこれで有効になる) | `PACKAGE_MANAGER` | スキップ (STATUS) |
| `ERGO_BUILD_SHURIKEN_MIGRATOR` | ON | `ergo_shuriken_migrator` と CLI (legacy。一覧に載せない理由は `module_list.md`) | `PARTICLE` + `GPU_PARTICLE` | スキップ (STATUS) |
| `ERGO_BUILD_HTTP` | **OFF** | `ergo_http`。ON にすると `ERGO_WITH_CURL` を強制する (取得対象の libcurl を伴うため既定 OFF) | 管理対象の libcurl (`CURL::libcurl`) | 構成エラー (取得が許されない・取得できないとき) |
| **付随ターゲット** | | | | |
| `ERGO_BUILD_PARTICLE_LINK` | ON | `ergo_particle_link` (`particle` プラグインから設定を受け取る受信専用の WS フィーダー) と `test_editor_link` | `PARTICLE` | 宣言されない |
| `ERGO_BUILD_SCENE_LINK` | ON | `ergo_scene_link` (`scene_editor` プラグインのエンジン側クライアント) と `test_scene_editor_link` | `SCENE` | 宣言されない |
| `ERGO_BUILD_LOG_LIVE` | ON | `ergo_log_live` (`LiveSink`: `ergo_log` の行を `log` プラグインへ WS で送る) と `test_live_sink` | `LOG` | 宣言されない |
| `ERGO_BUILD_PACKAGE_CLI` | ON | CLI `ergo_pkg` (ターゲット `ergo_pkg_cli`): `init` / `list` / `doctor` / `fetch` / `add` / `remove` / `update` / `status`。`outdated` / `watch` は `PACKAGE_WATCH`、Revisor / Concordia の読み手は `HTTP` があるときだけ | `PACKAGE_MANAGER` | スキップ (STATUS) |
| **モジュール固有ノブ** | | | | |
| `ERGO_AUDIO_BACKEND` | `auto` | `ergo_audio` のバックエンド: `auto` (FMOD を探し、無ければ dummy) / `fmod` (SDK 必須) / `dummy` (no-op 強制)。FMOD の場所は `FMOD_SDK_DIR` | `AUDIO` | `fmod` で SDK が無ければ構成エラー、`auto` は dummy (STATUS) |
| `ERGO_PARTICLE_HAS_RENDERER` | OFF | `ergo_particle` の Vulkan ビルボード描画 (`ERGO_PARTICLE_HAS_RENDERER=1`) | `PARTICLE`、`pictor` ターゲット + Vulkan SDK | Vulkan が無ければ STATUS (`renderer requested but Vulkan not found`)。`pictor` が無ければ何も出さずに描画なし |
| `ERGO_GPU_PARTICLE_COMPILE_SHADERS` | ON | `ergo_gpu_particle` の compute シェーダを `glslc` で SPIR-V に bake | `GPU_PARTICLE`、`glslc` | スキップ (STATUS `glslc not found — SPIR-V bake skipped`) |
| `ERGO_RENDER_REQUIRE_REAL` | OFF | 実描画経路 (`pictor` + Vulkan) を確保できないデスクトップ構成を構成エラーにする | `RENDER` | OFF のデスクトップは WARNING (`Vulkan-free build only`)。Android / iOS はこのノブに関係なく構成エラー |
| **管理対象の外部ライブラリ** ([third-party-deps.md](third-party-deps.md)) | | | | |
| `ERGO_FETCH_DEPENDENCIES` | ON | 依存を upstream から取得してよいかのマスタースイッチ (`cmake/ErgoDependencies.cmake`) | — | OFF で override も無いまま要求されると構成エラー |
| `ERGO_WITH_CURL` | OFF | libcurl を取得してビルド (`CURL::libcurl`、`third_party/dependencies.cmake`) | `ERGO_FETCH_DEPENDENCIES` か `ERGO_SOURCE_DIR_CURL` | 構成エラー |
| `ERGO_BUILD_THIRDPARTY_SMOKE` | OFF | 疎通テスト `test_thirdparty_deps` (curl を強制 ON) | `ERGO_BUILD_TESTS` | テストを作らない (curl は ON のまま取得・ビルドする) |
| `ERGO_CURL_USE_SSL` | ON | curl の TLS backend を有効化 (Windows は Schannel、`cmake/deps/curl.cmake`) | `ERGO_WITH_CURL` | 宣言されない |
| `ERGO_DEPENDENCY_CACHE_DIR` | (空) | 取得ソースの共有キャッシュ dir (空 = `<build>/_deps`) | — | — |
| `ERGO_SOURCE_DIR_<UPPER>` | (空) | その依存を取得せずローカルソースで差し替える (オフライン用、例 `ERGO_SOURCE_DIR_CURL`) | — | — |
| **LUDIARS パッケージ・ハブ** ([packages.md](packages.md) の「変数」) | | | | |
| `ERGO_PACKAGES` | (空) | 使う link パッケージの列 (`pictor;tela`)。空ならホストの宣言の `host.use`、それも無ければ互換モード M0 | — | — |
| `ERGO_PACKAGES_OPTIONAL` | (空) | 選んだうち、どの経路でも見つからなければ使わないもの (空なら `host.optional`) | — | — |
| `ERGO_PACKAGES_STRICT` | OFF | 警告扱いの検査結果を構成エラーにする (CI・出荷構成) | — | — |
| `ERGO_PACKAGES_FETCH` | OFF | 選んだパッケージを pin の SHA で取得してよい (ネットワーク) | — | — |
| `ERGO_PACKAGES_SIBLING_ROOT` | (空) | 兄弟チェックアウト `<root>/<Repo>` の探索を明示的に許す (P3 の使用関係図・`ergo_pkg watch` の取得元にも使う) | — | — |
| `ERGO_PACKAGE_MANIFEST_EXTRA` | (空) | ホストの宣言 (または旧来の追加マニフェスト) の場所。空ならホスト直下の `ergo-packages.json` が `host` 節を持つときそれ | — | — |
| `ERGO_PACKAGES_HOST_ID` | (空) | resolved.json の `host.id` (`host.code` が無いときだけ。空なら `CMAKE_PROJECT_NAME`) | — | — |
| `ERGO_PACKAGE_FEATURES_<NAME>` | (空) | 機能の足し引き (`+web;-vulkan`。空なら `host.features.<name>`) | — | — |
| `ERGO_PACKAGE_REV_<NAME>` | (空) | pin の一時上書き (検証用、PKG-06) | — | — |
| `ERGO_PACKAGE_SOURCE_DIR_<NAME>` | (空) | ローカルソースを明示 (worktree・オフライン) | — | — |
| `ERGO_PACKAGE_PROVIDED_<NAME>` | (空) | ホストが自分で作ると明示 (照合は行う) | — | — |
| **ホストのブートストラップ** (`ergo_pkg init` が書く `cmake/ergo-bootstrap.cmake`、ひな形は `cmake/packages/ergo-bootstrap.cmake.in`) | | | | |
| `ERGO_SOURCE_DIR` | (空) | ホストが取り込む Ergo のチェックアウト。空なら兄弟 `<host>/../<Repo>`、次に `ERGO_BOOTSTRAP_FETCH` | — | 指したディレクトリが Ergo でなければ構成エラー |
| `ERGO_BOOTSTRAP_FETCH` | OFF | チェックアウトが見つからないとき `host.ergo.rev` の Ergo を取得する (ネットワーク、全履歴) | — | 見つからず OFF なら構成エラー |

`<UPPER>` / `<NAME>` は依存名・パッケージ名を大文字にし `-` を `_` にしたもの
(`curl` → `CURL`、`pictor` → `PICTOR`)。

Ergo の中では `BUILD_SHARED_LIBS` を無視し、モジュールは常に STATIC
(`ergo: module libraries are always STATIC; BUILD_SHARED_LIBS ignored`)。ホスト自身の
`BUILD_SHARED_LIBS` / `BUILD_TESTING` は書き換えない。

### ビルド時に定義されるマクロ (フラグではなく自動分岐)

| マクロ | 条件 |
|---|---|
| `ERGO_RENDER_HAS_VULKAN=1` | `pictor` が `PICTOR_HAS_VULKAN` を公開し、`ErgoRenderBackend.cmake` が対象 platform の実描画経路を解決したとき |
| `ERGO_RENDER_PLATFORM_DESKTOP=1` / `ANDROID=1` / `IOS=1` | `ErgoRenderBackend.cmake` が解決した platform に対応する 1 つを定義 |
| `ERGO_AUDIO_BACKEND_FMOD=1` / `ERGO_AUDIO_BACKEND_DUMMY=1` | `ergo_audio` が実際に選んだバックエンド |
| `ERGO_TELA_GRAPH_HAS_TELA=1` / `ERGO_RENDER_VIZ_HAS_PICTOR=1` | `ergo_tela_graph_tela` / `ergo_render_viz_pictor` が作られたとき |

CMake の必須要件: バージョン `>= 3.20` (`cmake_minimum_required`)、C++17
(`ergo_math` / `ergo_physics2d` と上の C++20 アダプタは C++20)、`Threads`
(`find_package(Threads REQUIRED)`)。

### 構成時に読む環境変数

| 変数 | 読むもの | 役割 |
|---|---|---|
| `FMOD_SDK_DIR` | `cmake/FindFMOD.cmake` | FMOD Core SDK の場所 (同名のキャッシュ変数でもよい) |
| `LOCALAPPDATA` (Windows) / `XDG_STATE_HOME` / `HOME` | `cmake/packages/ErgoPackageWatchNotice.cmake` | `ergo_pkg watch` の状態ディレクトリ (`.../ergo/pkg-watch`) を探して、構成ログに更新の知らせを出す |

## CMake プリセット

`CMakePresets.json`。`binaryDir` はすべて `build/<プリセット名>`。

| プリセット | 種類 | 内容 | 詳細 |
|---|---|---|---|
| `vs2022` / `vs2022-deps` | configure + build (`-debug` / `-release`) | Visual Studio 2022。テスト・dummy・ベンチ OFF、audio は dummy。`-deps` は `ERGO_WITH_CURL` + `ERGO_BUILD_HTTP` | [ide-projects.md](ide-projects.md) |
| `xcode` / `xcode-deps` | configure + build (`-debug` / `-release`) | Xcode。内容は vs2022 系と同じ (`-deps` は OpenSSL の curl) | [ide-projects.md](ide-projects.md) |
| `ci-linux-gcc` | configure + build + test | Ninja、Release、`g++`、テスト ON、audio は dummy (Linux のみ) | [ci.md](ci.md) |
| `ci-linux-clang` | configure + build + test | Ninja、Release、`clang++`、`-Wall -Wextra` | [ci.md](ci.md) |
| `ci-asan` | configure + build + test | Ninja、Debug、ASan + UBSan。test プリセットが `ASAN_OPTIONS=detect_leaks=1` / `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1` を設定 | [ci.md](ci.md) |
| `ci-packages` | configure + build (Release) + test | `ERGO_PACKAGES=pictor;tela` + `ERGO_PACKAGES_FETCH` + `ERGO_PACKAGES_STRICT`、テスト ON、audio は dummy (ネットワークが要る。`packages-integration.yml` が使う) | [ci.md](ci.md) / [packages.md](packages.md) |

`ci-linux-*` / `ci-asan` は隠しプリセット `ci-base` (Ninja、`ERGO_BUILD_TESTS=ON`、
`ERGO_AUDIO_BACKEND=dummy`) を継承する。

## 環境変数 (tools/ergo)

`tools/ergo/` の Node サーバ (と Electron) が `process.env` から読む変数の全部。
**秘密情報は `ANTHROPIC_API_KEY` だけ**で、シェルの環境変数として渡し、ファイルに
書いてコミットしない。

| 変数 | 既定 | 読むもの | 役割 | 出典 |
|---|---|---|---|---|
| `PORT` | `5170` | サーバ全体 | listen ポート (Electron とヘッドレスの両方) | `tools/ergo/src/main.ts:10` / `tools/ergo/electron/main.cjs:17` |
| `ERGO_HOST` | `127.0.0.1` | サーバ全体 (ヘッドレスのみ) | bind するアドレス。LAN に出したいときだけ `0.0.0.0` などにする。Electron (`npm start`) は読まない | `tools/ergo/src/main.ts:11` / `tools/ergo/src/core/server.ts:291` |
| `ERGO_PLUGIN_DIR` | (なし) | 外部プラグインの読み込み | ゲーム固有の plugin pack のディレクトリ。OS のパス区切りで複数可 ([external-plugins.md](external-plugins.md)) | `tools/ergo/src/core/external.ts:90` |
| `ERGO_PICTOR_PROFILE_DIR` | `../../../Pictor/profiles` → `./profiles` (cwd 相対) | `render_pipeline` | `*.profile.json` の場所 ([plugin-data-roots.md](plugin-data-roots.md)) | `tools/ergo/src/plugins/render_pipeline/profile_store.ts:31` |
| `VISUS_PROJECT_ROOTS` | `process.cwd()` | `visus` | 描画定義のプロジェクトルート。`;` 区切りで複数可 | `tools/ergo/src/plugins/visus/index.ts:48` |
| `ERGO_VFX_BACKEND` | `cli` | `vfx-suggest` / `vfx-breed` | LLM の呼び方: `cli` (Claude CLI) / `sdk` (Anthropic SDK)。それ以外はエラー | `tools/ergo/src/plugins/vfx-suggest/generator.ts:24` / `tools/ergo/src/plugins/vfx-breed/advisor.ts:33` |
| `ERGO_VFX_MODEL` | `haiku` (`cli`) / `claude-haiku-4-5` (`sdk`) | `vfx-suggest` / `vfx-breed` | 使うモデル | `tools/ergo/src/plugins/vfx-suggest/generator.ts:25` / `tools/ergo/src/plugins/vfx-breed/advisor.ts:34` |
| `CLAUDE_BIN` | `claude` | `vfx-suggest` / `vfx-breed` (`cli`) | Claude CLI の実行ファイル | `tools/ergo/src/plugins/vfx-suggest/generator-cli.ts:13` / `tools/ergo/src/plugins/vfx-breed/advisor-cli.ts:11` |
| `ANTHROPIC_API_KEY` | (なし) | `vfx-suggest` / `vfx-breed` (`sdk`) | **秘密情報**。シェルの環境変数でだけ渡す (ファイル・設定に書かない、コミットしない) | `tools/ergo/src/plugins/vfx-suggest/generator-sdk.ts:13` / `tools/ergo/src/plugins/vfx-breed/advisor-sdk.ts:10` |

LLM が無い・失敗したときも `vfx-suggest` / `vfx-breed` 以外のプラグインは影響を受けず、
`vfx-breed` の進化は方向指示なしで動く。CI の `tools-ergo` ジョブは `npm ci` の前に
`ELECTRON_SKIP_BINARY_DOWNLOAD=1` を設定する (npm の依存取得用で、サーバは読まない)。

## 環境変数 (`ergo_pkg`)

`ergo_pkg` (CLI) が実行時に読む変数。コマンドラインの引数があればそちらが優先する。
パッケージ・ハブの構成時の変数 (`ERGO_PACKAGES` ほか) は上の表と
[packages.md](packages.md) の「変数」を参照。

| 変数 | 使うコマンド | 優先順位と役割 | 出典 |
|---|---|---|---|
| `ERGO_PKG_CACHE_DIR` | `outdated` / `watch` | ツリーレス bare ミラーの置き場。`--cache` > `ERGO_PKG_CACHE_DIR` > `ERGO_DEPENDENCY_CACHE_DIR` > 一時ディレクトリの `ergo_pkg` | `tools/package_cli/watch_paths.cpp:39` / `tools/package_cli/git_rev_resolver.cpp:44` |
| `ERGO_DEPENDENCY_CACHE_DIR` | `fetch` (と上のミラー) | 取得先。`--cache` > `--build` の `FETCHCONTENT_BASE_DIR` (無ければ `<build>/_deps`) > `ERGO_DEPENDENCY_CACHE_DIR` | `tools/package_cli/cmd_fetch.cpp:55` |
| `ERGO_SOURCE_DIR` | 全コマンド | 使う Ergo のチェックアウト。`--ergo` > `ERGO_SOURCE_DIR` > `ergo_pkg` をビルドした Ergo | `tools/package_cli/host_context.cpp:42` |
| `ERGO_PACKAGES_SIBLING_ROOT` | `outdated` / `watch` | 兄弟チェックアウト `<root>/<Repo>` を取得元にする。`--root` > `ERGO_PACKAGES_SIBLING_ROOT` | `tools/package_cli/watch_paths.cpp:43` |
| `ERGO_WATCH_REVISOR_URL` / `ERGO_WATCH_CONCORDIA_URL` | `watch` | Revisor / Concordia の URL (loopback だけ受け付ける)。`--revisor-url` / `--concordia-url` が優先。`-DERGO_BUILD_HTTP=ON` で作った `ergo_pkg` だけが読む | `tools/package_cli/provenance_endpoints.cpp:31-32` |
| `LOCALAPPDATA` (Windows) / `XDG_STATE_HOME` / `HOME` | `watch` | 状態ディレクトリ (`.../ergo/pkg-watch`)。`--state` が優先 | `tools/package_cli/watch_paths.cpp:30-33` |

`ERGO_BOOTSTRAP_FETCH` と、ホストのブートストラップの `ERGO_SOURCE_DIR` は環境変数では
なくホストの CMake キャッシュ変数 (上の表の「ホストのブートストラップ」)。

## ポート

| ポート | 用途 | 上書き |
|---|---|---|
| `5170` | tools/ergo シェル + 全プラグイン (HTTP / WS)。エンジン側のクライアント (`ergo_bind` / `ergo_particle_link` / `ergo_scene_link` / `ergo_log_live`) も既定でここへつなぐ | `PORT` 環境変数 (クライアント側は各 `connect` の引数) |

> 旧 `particle-editor` (5173) / `variable-editor` (5174) は廃止済みで、すべて
> 5170 に統合 ([`../feature/tool/ergo.md`](../feature/tool/ergo.md) 「互換性」)。

## 関連ガイド

- C++ ビルド: [build-cpp.md](build-cpp.md)
- ツール起動: [run-tools-ergo.md](run-tools-ergo.md)
- 外部プラグイン: [external-plugins.md](external-plugins.md)
- プラグイン資産ルート: [plugin-data-roots.md](plugin-data-roots.md)
- CI と手元での再現: [ci.md](ci.md)
- ブランチ + PR 運用: [branch-and-pr.md](branch-and-pr.md)
