# C++ モジュールをビルドするための設定

## 目的

`ergo_<domain>` の C++ モジュール (例: `ergo_input` / `ergo_render` /
`ergo_particle`) をビルド・テストする。Ergo はトップレベル `CMakeLists.txt`
が全モジュールを `add_library(ergo_<name>)` として公開する単一 CMake ツリーで、
**premake は使わない**。ホストアプリは `add_subdirectory(<ergo>)` で取り込む
(下記「ホストアプリからの取り込み」)。

## 前提

- CMake `>= 3.20` (`cmake_minimum_required`)
- C++17 が基準 (`CMAKE_CXX_STANDARD 17`、extensions OFF)。例外は
  `ergo_math` (C++20 concepts) と、それをリンクする `ergo_physics2d` で C++20。
  どちらの標準も各ターゲットの usage requirement (`target_compile_features`) として
  利用側へ伝播する (`cmake/ErgoTargets.cmake`)
- `Threads` は必須依存 (`find_package(Threads REQUIRED)`)

## ビルドフラグ (CMake オプション)

`-D<KEY>=<値>` で渡す。全オプションの一覧と既定値の正本は
[`config-reference.md`](config-reference.md)。ここではビルドの形とモジュールの
組み合わせに関わるものだけを扱う。

### 全体スイッチ

| キー | 既定 | 役割 |
|---|---|---|
| `ERGO_BUILD_TESTS`               | トップレベル: ON / サブプロジェクト: OFF | モジュールテストをビルドする (CTest 登録) |
| `ERGO_BUILD_EXAMPLES`            | トップレベル: ON / サブプロジェクト: OFF | `examples/` のヘッドレスな使用例をビルドする ([integrate-modules.md](integrate-modules.md)) |
| `ERGO_EXCLUDE_FROM_ALL`          | サブプロジェクトで ON / トップレベル OFF。`ERGO_BUILD_TESTS=ON` の間は効かない | Ergo のターゲットを既定ビルド (`all`) から外す。ホストがリンクしたものだけがビルドされる |
| `ERGO_POSITION_INDEPENDENT_CODE` | ON  | Ergo のライブラリを PIC でビルドする (ホストの共有ライブラリ / プラグインにリンクできる) |
| `ERGO_BUILD_DUMMY`               | OFF | dummy プラグライブラリ (no-op 実装) をビルドする |
| `ERGO_BUILD_BENCHMARKS`          | OFF | マイクロベンチ実行ファイルをビルドする (opt-in) |

「トップレベル」は Ergo 自身を `cmake -S <ergo>` で構成した場合、
「サブプロジェクト」はホストの `add_subdirectory` / FetchContent /
`ergo_pkg` のブートストラップで取り込まれた場合 (判定は
`CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR`)。

### モジュール個別スイッチ (`ERGO_BUILD_<名>`)

不要なモジュールは `-DERGO_BUILD_<名>=OFF` で外せる (例 `-DERGO_BUILD_AUDIO=OFF`)。
一覧と既定は [`config-reference.md`](config-reference.md)。

モジュール間の前提は構成時に検査する。リンク時 (`cannot find -lergo_io` など) まで
遅らせない:

| モジュール | 前提 | 前提が OFF のとき |
|---|---|---|
| `PHYSICS2D` | `MATH` | 構成エラー |
| `UI_KIT` / `UI_LAYOUT` / `RESOURCE` | `IO` | 構成エラー |
| `HTTP` | 管理対象の libcurl (`ERGO_WITH_CURL`、`HTTP=ON` で自動 ON) | 取得できなければ構成エラー |
| `ACTOR` | `BIND` | スキップ (`ergo_actor: requires ERGO_BUILD_BIND — skipped`) |
| `CAST` | `ACTOR` + `BIND` | スキップ (STATUS) |
| `SHURIKEN_MIGRATOR` | `PARTICLE` + `GPU_PARTICLE` | スキップ (STATUS) |
| `RENDER_VIZ` / `GRAPH_LAYOUT` | `TELA_GRAPH` | スキップ (STATUS) |
| `PACKAGE_VIZ` | `PACKAGE_MODEL` + `PACKAGE_SCAN` + `GRAPH_LAYOUT` | スキップ (STATUS) |
| `PACKAGE_WATCH` / `PACKAGE_CLI` | `PACKAGE_MANAGER` | スキップ (STATUS) |

構成エラーは欠けている組み合わせをすべて 1 つのメッセージに並べる
(`ERGO_BUILD_UI_KIT=ON but ERGO_BUILD_IO=OFF. ... No silent fallback.`)。
検査は `cmake/ErgoModuleRequirements.cmake`。

### モジュール固有ノブ

| キー | 既定 | 役割 |
|---|---|---|
| `ERGO_AUDIO_BACKEND`              | `auto` | `ergo_audio` のバックエンド。`auto` (FMOD 検出 → 無ければ dummy) / `fmod` (SDK 必須・無ければエラー) / `dummy` (no-op 強制) |
| `ERGO_PARTICLE_HAS_RENDERER`      | OFF    | `ergo_particle` の Vulkan ビルボード描画を有効化。`pictor` ターゲット + Vulkan SDK が必要 |
| `ERGO_GPU_PARTICLE_COMPILE_SHADERS` | ON   | `ergo_gpu_particle` の GLSL compute を `glslc` で SPIR-V に bake する |
| `ERGO_RENDER_REQUIRE_REAL`        | OFF    | `ergo_render` の実描画経路を確保できないデスクトップ構成を構成エラーにする。Android / iOS は常に必須なのでこのノブに関係なくエラー |

## Pictor / Vulkan 連携 (描画系モジュール)

`ergo_render` と `ergo_particle` の **実描画パス** は Pictor + Vulkan に依存する
が、専用の有効化フラグは無く、**`pictor` CMake ターゲットが存在するか**で
自動分岐する。`pictor` ターゲットはホスト統合側 (例: AdventureCube /
PrivateGame のスーパープロジェクト、モバイルホスト) が
`add_subdirectory(Pictor ...)` 等で提供する。Ergo 単体ビルドでは通常
Vulkan 非依存ビルドになる。正本は `CMakeLists.txt` の `ergo_particle` /
`ergo_render` 各セクション。

### ergo_render のクロスプラットフォーム契約 (KD-MOB-001)

判定の正本は **`pictor` ターゲットが公開する `PICTOR_HAS_VULKAN`** であり、
デスクトップ専用の import ターゲット `Vulkan::Vulkan` の有無ではない。
Android は NDK 同梱 `libvulkan.so`、iOS は MoltenVK を pictor 側がリンクする
ため、モバイルでは `find_package(Vulkan)` が失敗する。ここを desktop 前提で
判定するとモバイルが黙って Vulkan-free ビルドへ落ちる。判定は
`cmake/ErgoRenderBackend.cmake` に集約した。

| プラットフォーム | Vulkan の出所 | ergo_render のリンク | 付与される定義 |
|---|---|---|---|
| Desktop (SDK) | Vulkan SDK (`find_package(Vulkan)`) | `pictor` + `Vulkan::Vulkan` | `ERGO_RENDER_PLATFORM_DESKTOP=1` |
| Desktop (host supplied) | pictor が供給する Vulkan | `pictor` のみ | `ERGO_RENDER_PLATFORM_DESKTOP=1` |
| Android (`-DANDROID=ON` / NDK toolchain) | NDK 同梱 `libvulkan.so` | `pictor` のみ | `ERGO_RENDER_PLATFORM_ANDROID=1` |
| iOS (`CMAKE_SYSTEM_NAME=iOS`) | MoltenVK | `pictor` のみ | `ERGO_RENDER_PLATFORM_IOS=1` |

有効化時は共通で `ERGO_RENDER_HAS_VULKAN=1` と
`ERGO_RENDER_VULKAN_SOURCE="<出所>"` が付く。出所名は空白を含むため、定義は
`ergo_render_vulkan_source_definition()` で作る。値のエスケープは CMake に任せ、
CMake 文字列で `\"` を 1 段だけ書く (二重に書くと MSVC で C2017 / C2001 になる)。
この関数の出力は `test_render_vulkan_source_define` が実描画の有無に関係なく検査する。
実描画を確保できなかった場合:

- **Android / iOS は常に構成エラー** (`FATAL_ERROR`)。モバイルに Vulkan-free の
  正当な用途は無いため、黙って縮退させない。
- **Desktop は既定で警告**のみ (Vulkan 非依存部分のユニットテスト構成を残す)。
  `-DERGO_RENDER_REQUIRE_REAL=ON` で構成エラーへ格上げできる。
- `glslc` の有無は SPIR-V bake の可否だけに影響し、実描画可否には影響しない
  (モバイルは bake 済み SPIR-V をホストのパッケージが配る)。

実行時にも同じ契約を型付きで問い合わせられる — `render_backend_contract()` /
`check_render_requirements()` / `FrameComposer::initialize()` の戻り値
(`RenderBackendError`)。詳細は
[`../feature/module/render.md`](../feature/module/render.md) (F)〜(H)。

`ergo_particle` は従来どおり `pictor` + `find_package(Vulkan)` で分岐する
(desktop 前提のまま。モバイル対応は後続タスク)。正本は
`CMakeLists.txt` の `ergo_particle` / `ergo_render` 各セクションと
`cmake/ErgoRenderBackend.cmake`。

## 手順

Visual Studio の `.sln` / Xcode の `.xcodeproj` を生成する場合は
[IDE プロジェクト生成](ide-projects.md) のプリセットを使う。
libcurl を含む構成と、ホストアプリの雛形もそちらに記載する。

```bash
# 1. 構成 (例: テスト込み Release、audio は dummy 固定)
cmake -S . -B build -DERGO_AUDIO_BACKEND=dummy

# 2. ビルド
cmake --build build --config Release

# 3. テスト
ctest --test-dir build -C Release
```

## ホストアプリからの取り込み

```cmake
add_subdirectory(<path-to-ergo>)
target_link_libraries(myapp PRIVATE Ergo::input Ergo::bind)
```

`ergo_pkg init` が生成するブートストラップ経由でも同じ扱いになる
([`packages.md`](packages.md))。ホスト側で次の設定を書き足す必要はない:

- **ターゲット名**: `Ergo::<名>` (`ergo_<名>` の ALIAS) を推奨する。全 `ergo_*`
  ライブラリに付く (`Ergo::log`、`Ergo::physics2d`、`Ergo::thirdparty` など)。
  綴り違いや OFF のモジュールは構成時エラーになる (素の `ergo_<名>` は
  `-lergo_<名>` としてリンク時まで残る)。素の名前も従来どおり使える。
- **サブプロジェクトの既定**: `ERGO_BUILD_TESTS=OFF` (Ergo のテストを登録しない) と
  `ERGO_EXCLUDE_FROM_ALL=ON` (ホストがリンクしたライブラリだけをビルドする)。
  ホストの `ctest` に Ergo のテストは出ず、ホストのターゲット名 (`test_math` 等) とも
  衝突しない。既存のビルドディレクトリはキャッシュの `ERGO_BUILD_TESTS=ON` を
  持ち続けるので、ビルドディレクトリを作り直すか `-DERGO_BUILD_TESTS=OFF` を渡す
  (テスト ON の間は `ERGO_EXCLUDE_FROM_ALL` は効かない。`ERGO_BUILD_TESTS` は既存の
  ビルドディレクトリでどちら向きに切り替えても反映される)。明示的に上書きしたい場合は
  `set(ERGO_BUILD_TESTS ON CACHE BOOL "" FORCE)` のように `add_subdirectory` の前で設定する。
- **C++ 標準**: 各ライブラリが `cxx_std_17` (`ergo_math` は `cxx_std_20`) を PUBLIC に
  持つので、ホストが C++14 (MSVC の既定) でもリンクしたターゲットは自動で
  引き上がる。`ergo_physics2d` / `ergo_math` をリンクすると C++20 になる。
- **MSVC の `/utf-8`**: 公開ヘッダは BOM なし UTF-8 の日本語コメントを含むため、
  全ライブラリが `/utf-8` を PUBLIC に持つ (ホストに伝播する)。
- **静的ライブラリのみ・PIC**: Ergo のライブラリは常に STATIC。Ergo には export
  マクロ (dllexport) が無いため、ホストの `BUILD_SHARED_LIBS=ON` は Ergo の中では
  無視する (`ergo: module libraries are always STATIC; BUILD_SHARED_LIBS ignored`)。
  ホスト自身の `BUILD_SHARED_LIBS` は変えない。既定で PIC なので、ホストの
  共有ライブラリ / プラグイン (`add_library(... SHARED)`) にもリンクできる。
- **キャッシュ**: 管理対象の libcurl を ON にしても、ホストの `BUILD_SHARED_LIBS` /
  `BUILD_TESTING` は書き換えない ([`third-party-deps.md`](third-party-deps.md))。
- **FMOD ランタイムの配置**: `ergo_audio_stage_runtime(<exe>)` をホストの
  `CMakeLists.txt` から呼ぶ ([`../feature/module/audio.md`](../feature/module/audio.md))。

回帰テスト: `test_host_consume` (C++14 + `BUILD_SHARED_LIBS=ON` のホストが
`Ergo::input` / `Ergo::log` / `Ergo::bind` / `Ergo::physics2d` と SHARED プラグインを
ビルドする)、`test_host_cache_isolation` (curl ON でホストのキャッシュが残る)、
`test_audio_stage_runtime`。いずれも実際にホストプロジェクトを構成・ビルドする
(ctest ラベル `host`)。

## 注意点

- **`glslc` が無い環境** では SPIR-V bake がスキップされるだけでビルド自体は
  通る (`ERGO_GPU_PARTICLE_COMPILE_SHADERS=ON` でも非致命)。
- **MSVC で日本語コメントを含むヘッダ**: `/utf-8` は全ライブラリの usage
  requirement なので、リンクしたターゲットには自動で付く。Ergo のライブラリを
  リンクせずにヘッダだけを読むターゲットは自分で `/utf-8` を付ける。
- 新規モジュール追加時は `CMakeLists.txt` への `add_library` 追加 +
  `module_list.md` / `module_list.yaml` / README / [config-reference.md](config-reference.md) /
  `spec/domains` の更新が必要で、漏れは ctest `test_module_catalog` が検出する (手順は
  [`../../CLAUDE.md`](../../CLAUDE.md) 「新規モジュール追加手順」)。`Ergo::<名>`
  エイリアス・C++ 標準・`/utf-8` は `cmake/ErgoTargets.cmake` が全 `ergo_*`
  ライブラリに付けるので、モジュール側で書く必要はない。別のモジュールが無いと
  成り立たないモジュールは `cmake/ErgoModuleRequirements.cmake` の表に足す。
