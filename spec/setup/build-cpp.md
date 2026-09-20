# C++ モジュールをビルドするための設定

## 目的

`ergo_<domain>` の C++17 モジュール (例: `ergo_input` / `ergo_render` /
`ergo_particle`) をビルド・テストする。Ergo はトップレベル `CMakeLists.txt`
が全モジュールを `add_library(ergo_<name>)` として公開する単一 CMake ツリーで、
**premake は使わない**。ホストアプリは `add_subdirectory(<ergo>)` で取り込む
(詳細は [`../../README.md`](../../README.md) 「ホストアプリからの利用」)。

## 前提

- CMake `>= 3.16` (`cmake_minimum_required` / `CMakeLists.txt:1`)
- C++17 (`CMAKE_CXX_STANDARD 17`、extensions OFF / `CMakeLists.txt:4-6`)
- `Threads` は必須依存 (`find_package(Threads REQUIRED)` / `CMakeLists.txt:50`)

## ビルドフラグ (CMake オプション)

すべて `CMakeLists.txt` の `option()` / `set(... CACHE)` で定義。`-D<KEY>=<値>`
で渡す。

### 全体スイッチ

| キー | 既定 | 役割 |
|---|---|---|
| `ERGO_BUILD_TESTS`      | ON  | モジュールテストをビルドする (CTest 登録) |
| `ERGO_BUILD_DUMMY`      | ON  | dummy プラグライブラリ (no-op 実装) をビルドする |
| `ERGO_BUILD_BENCHMARKS` | OFF | マイクロベンチ実行ファイルをビルドする (opt-in) |

### モジュール個別スイッチ (`ERGO_BUILD_<名>`、すべて既定 ON)

`INPUT` / `PARTICLE` / `GPU_PARTICLE` / `BIND` / `ACTOR` / `SOUND` / `FRAME` /
`PROFILE` / `LOG` / `IO` / `AUDIO` / `WORLD_TIME` / `BLACKBOARD` / `UI` /
`CUSTOS` / `HEALTH` / `SCORE` / `COMBO_COUNTER` / `TIMING_JUDGE` / `UI_KIT` /
`SHURIKEN_MIGRATOR` / `RENDER`。

不要なモジュールは `-DERGO_BUILD_<名>=OFF` で外せる
(例 `-DERGO_BUILD_AUDIO=OFF`)。正本は `CMakeLists.txt:15-36`。

### モジュール固有ノブ

| キー | 既定 | 役割 |
|---|---|---|
| `ERGO_AUDIO_BACKEND`              | `auto` | `ergo_audio` のバックエンド。`auto` (FMOD 検出 → 無ければ dummy) / `fmod` (SDK 必須・無ければエラー) / `dummy` (no-op 強制) (`CMakeLists.txt:45`) |
| `ERGO_PARTICLE_HAS_RENDERER`      | OFF    | `ergo_particle` の Vulkan ビルボード描画を有効化。`pictor` ターゲット + Vulkan SDK が必要 (`CMakeLists.txt:39`) |
| `ERGO_GPU_PARTICLE_COMPILE_SHADERS` | ON   | `ergo_gpu_particle` の GLSL compute を `glslc` で SPIR-V に bake する (`CMakeLists.txt:40`) |
| `ERGO_RENDER_REQUIRE_REAL`        | OFF    | `ergo_render` の実描画経路を確保できないデスクトップ構成を構成エラーにする。Android / iOS は常に必須なのでこのノブに関係なくエラー |

## Pictor / Vulkan 連携 (描画系モジュール)

`ergo_render` と `ergo_particle` の **実描画パス** は Pictor + Vulkan に依存する
が、専用の有効化フラグは無く、**`pictor` CMake ターゲットが存在するか**で
自動分岐する。`pictor` ターゲットはホスト統合側 (例: AdventureCube /
PrivateGame のスーパープロジェクト、モバイルホスト) が
`add_subdirectory(Pictor ...)` 等で提供する。Ergo 単体ビルドでは通常
Vulkan 非依存ビルドになる。正本は `CMakeLists.txt:118-160` (particle) /
`CMakeLists.txt:695-758` (render)。

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
`ERGO_RENDER_VULKAN_SOURCE="<出所>"` が付く。実描画を確保できなかった場合:

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
kazmath / libcurl を含む構成と、ホストアプリへのリンク例もそちらに記載する。

```bash
# 1. 構成 (例: テスト込み Release、audio は dummy 固定)
cmake -S . -B build -DERGO_AUDIO_BACKEND=dummy

# 2. ビルド
cmake --build build --config Release

# 3. テスト
ctest --test-dir build -C Release
```

ホストアプリから取り込む場合:

```cmake
add_subdirectory(<path-to-ergo>)
target_link_libraries(myapp PRIVATE ergo_input ergo_bind)
```

## 注意点

- **`glslc` が無い環境** では SPIR-V bake がスキップされるだけでビルド自体は
  通る (`ERGO_GPU_PARTICLE_COMPILE_SHADERS=ON` でも非致命)。
- **MSVC で日本語コメントを含むヘッダ** を consumer 側でビルドする場合、
  `/utf-8` が consumer に伝播しない既知の罠がある。`ergo_render` は
  `target_compile_options(... PUBLIC /utf-8)` を付けている (`CMakeLists.txt:721`)
  が、テストターゲット等は個別に `/utf-8` を要する場合がある。
- 新規モジュール追加時は `CMakeLists.txt` への `add_library` 追加 +
  `module_list.md` / `module_list.yaml` 更新が必要 (手順は
  [`../../CLAUDE.md`](../../CLAUDE.md) 「新規モジュール追加手順」)。
