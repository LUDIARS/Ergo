# Visual Studio / Xcode プロジェクトの生成

Ergo の CMake ターゲットを正本とし、`CMakePresets.json` から IDE のネイティブ
プロジェクトを生成する。生成物は `build/<preset>/` に置き、Git には含めない。
各プリセットは独立したビルドツリーを使うため、ジェネレータの混在を避けられる。

## 前提

- プリセット利用時は CMake 3.21 以上。従来の `cmake -S . -B build` は引き続き
  CMake 3.20 以上で利用できる。
- Windows: Visual Studio 2022 の「C++ によるデスクトップ開発」と Windows SDK。
- macOS: Xcode 本体と、Xcode 用に選択された developer directory。
  Command Line Tools だけでは Xcode ジェネレータを利用できない。
- `-deps` 構成: Git と依存ソースへのアクセス、または下記のローカルソース指定。
  macOS の libcurl/HTTPS には OpenSSL の開発用ヘッダ・ライブラリも必要。

## Ergo のプロジェクトを生成

Ergo リポジトリのルートで実行する。

| 用途 | 生成コマンド | 開くファイル |
|---|---|---|
| VS 2022 x64 | `cmake --preset vs2022` | `build/vs2022/Ergo.sln` |
| VS + libcurl | `cmake --preset vs2022-deps` | `build/vs2022-deps/Ergo.sln` |
| Xcode | `cmake --preset xcode` | `build/xcode/Ergo.xcodeproj` |
| Xcode + libcurl | `cmake --preset xcode-deps -DOPENSSL_ROOT_DIR=/absolute/path/to/openssl` | `build/xcode-deps/Ergo.xcodeproj` |

生成コマンドはプロジェクトを構成するだけで、Ergo のビルド・実行・CTest は行わない。
CMake 自身によるコンパイラ・SDK 検出は行われる。IDE では Debug / Release /
RelWithDebInfo を選択できる。コマンドラインでビルドする場合は
`cmake --build --preset vs2022-deps-debug` や
`cmake --build --preset xcode-deps-release` を使う。
`CMAKE_BUILD_TYPE` ではなく構成選択を使う。

プリセットはテスト・dummy ライブラリ群・ベンチマークの生成を無効にし、
audio バックエンドは明示的に `dummy` を選択する。
実音声には `-DERGO_AUDIO_BACKEND=fmod -DFMOD_SDK_DIR=/absolute/path/to/fmod-sdk`
を指定する。必要な SDK がなければ構成エラーになる。

## 周辺ライブラリのリンク

`-deps` プリセットは既存の固定リビジョン管理を使って static libcurl を
同じ IDE プロジェクト内に取り込む。`ergo_http` も有効にし、libcurl へリンクする。
include パス・コンパイル定義・システムライブラリ・ビルド順は CMake ターゲットが
伝播するため、IDE で `.lib` / `.a` のパスを手作業で設定しない。

| 利用対象 | ホストがリンクするターゲット | 補足 |
|---|---|---|
| Ergo モジュール | `Ergo::input`、`Ergo::bind` など (`ergo_input` 等の素の名前も可) | Threads / Windows sockets 等も既存の依存設定から伝播。C++ 標準と MSVC の `/utf-8` も伝播 |
| HTTP | `Ergo::http` | libcurl への依存を伝播。Windows TLS は Schannel、Xcode 依存構成は OpenSSL |
| 管理対象ライブラリ一式 | `Ergo::thirdparty` (旧名 `ergo::thirdparty` も可) | 有効にした libcurl をまとめてリンク |
| FMOD | `Ergo::audio` | `FMOD::FMOD` を伝播。DLL / .so / .dylib は `ergo_audio_stage_runtime(<exe>)` で exe の隣へ配置できる。macOS の rpath / 署名はホストで設定 |
| Pictor / Vulkan | `Ergo::render`、`Ergo::particle` | ホストが先に `pictor` ターゲットを提供し、Vulkan SDK を設定する既存契約を使用 |

基本プリセットは管理対象ライブラリを取得しない。Pictor は自動取得しない。
描画統合の条件は [build-cpp.md](build-cpp.md)、依存の固定リビジョンと
ライセンスは [third-party-deps.md](third-party-deps.md) を参照。

オフラインで依存込みプロジェクトを生成する例:

```sh
cmake --preset vs2022-deps -DERGO_FETCH_DEPENDENCIES=OFF -DERGO_SOURCE_DIR_CURL=<deps>/curl
```

Xcode も同じ変数を使える。ソース指定が欠けた依存はエラーになり、無言で除外しない。
異なるジェネレータ間で `ERGO_DEPENDENCY_CACHE_DIR` を共有すると依存側のビルド
キャッシュも競合し得るため、IDE 間では既定の独立した `_deps` を利用する。

## 新しいホストアプリを作る

ホストの `external/ergo` に Ergo の固定リビジョンを取得し、次のような
`CMakeLists.txt` とアプリの `main.cpp` を置く。マシン固有の sibling パスに依存させない。

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyGame LANGUAGES C CXX)
set(ERGO_AUDIO_BACKEND dummy CACHE STRING "" FORCE)
set(ERGO_WITH_CURL ON CACHE BOOL "" FORCE)
set(ERGO_BUILD_HTTP ON CACHE BOOL "" FORCE)
add_subdirectory(external/ergo)
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE Ergo::input Ergo::bind Ergo::http Ergo::thirdparty)
```

サブプロジェクトとして取り込まれた Ergo は、テストを登録せず
(`ERGO_BUILD_TESTS=OFF`)、ホストがリンクしたライブラリだけをビルドする
(`ERGO_EXCLUDE_FROM_ALL=ON`)。C++ 標準 (C++17、`ergo_math` / `ergo_physics2d` は
C++20) と MSVC の `/utf-8` はリンクしたターゲットへ伝播するので、ホスト側で
`target_compile_features` や `/utf-8` を書く必要はない。既定を明示的に変えたい
場合だけ `add_subdirectory` の前で上書きする
(例: Ergo のテストもホストでビルドする `set(ERGO_BUILD_TESTS ON CACHE BOOL "" FORCE)`)。
詳細は [build-cpp.md](build-cpp.md)「ホストアプリからの取り込み」。

ホストのルートで、Windows は
`cmake -S . -B build/vs2022 -G "Visual Studio 17 2022" -A x64`、macOS は
`cmake -S . -B build/xcode -G Xcode -DOPENSSL_ROOT_DIR=/absolute/path/to/openssl`
で生成する。出力はそれぞれ `MyGame.sln` / `MyGame.xcodeproj`。
Ergo のプリセットは Ergo 自身用なので、ホスト側は自分のプリセットを管理する。

## 個人設定

SDK パスや別 architecture は Git 対象外の `CMakeUserPresets.json` に保持できる。
例: `xcode-deps` を継承して Apple Silicon を明示する。

```json
{
  "version": 3,
  "configurePresets": [{
    "name": "xcode-local",
    "inherits": "xcode-deps",
    "cacheVariables": {
      "CMAKE_OSX_ARCHITECTURES": "arm64",
      "OPENSSL_ROOT_DIR": "/absolute/path/to/openssl"
    }
  }]
}
```

`cmake --preset xcode-local` で `build/xcode-local/Ergo.xcodeproj` を生成する。
architecture を変える場合は OpenSSL / FMOD 等も同じ architecture のものを用意する。

仕様参照: [CMake Presets](https://cmake.org/cmake/help/v3.22/manual/cmake-presets.7.html)、
[Visual Studio generator](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2017%202022.html)、
[Xcode generator](https://cmake.org/cmake/help/latest/generator/Xcode.html)。
