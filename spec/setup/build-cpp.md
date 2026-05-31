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

## Pictor / Vulkan 連携 (描画系モジュール)

`ergo_render` と `ergo_particle` の **実描画パス** は Pictor + Vulkan に依存する
が、専用の有効化フラグは無く、**`pictor` CMake ターゲットが存在するか**で
自動分岐する:

- `pictor` ターゲットが無い → Vulkan 非依存部分のみビルド (カメラ math /
  asset path / ScreenshotBridge 等。`ERGO_RENDER_HAS_VULKAN` 未定義の縮退型)
- `pictor` ターゲットあり + `find_package(Vulkan)` 成功 → 実描画パスを有効化
  (`ERGO_RENDER_HAS_VULKAN=1` を定義、`glslc` があれば SPIR-V を bake)

`pictor` ターゲットはホスト統合側 (例: AdventureCube / KuzuSurvivors の
スーパープロジェクト) が `add_subdirectory(Pictor ...)` 等で提供する。Ergo 単体
ビルドでは通常 Vulkan 非依存ビルドになる。正本は `CMakeLists.txt:118-160`
(particle) / `CMakeLists.txt:695-758` (render)。

## 手順

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
