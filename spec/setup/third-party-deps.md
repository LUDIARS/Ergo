# サードパーティ・ライブラリ管理 (kazmath / curl / ...)

## 目的

kazmath・curl のような **コンパイルを伴う外部 C/C++ ライブラリ** を、ピン留め
した upstream から取得 (fetch) してビルドし、Ergo の任意モジュール / ホストから
使えるようにする仕組み。再現性 (= 固定 commit/tag) と「無言フォールバック禁止」
を満たすことを最優先にした宣言的マネージャ。

> ヘッダオンリーで verbatim にベンダリングしているライブラリ
> (`third_party/earcut`・`third_party/nanosvg`・`third_party/gtest`) は対象外。
> それらは従来どおり `third_party/<name>/` にコピーで置き、`THIRD_PARTY_NOTICES.md`
> に attribution を載せる。本マネージャは「取得してビルドする依存」専用。

> 用語注意: ここで言う「サードパーティ」は **C/C++ ライブラリ** を指す。
> `tools/ergo` の Web エディタ拡張 (`ERGO_PLUGIN_DIR` / plugin pack) とは別物。
> そちらは [`external-plugins.md`](external-plugins.md) を参照。

## 構成 (SRP で 3 層)

| ファイル | 役割 |
|---|---|
| `cmake/ErgoDependencies.cmake` | **エンジン**。fetch / populate / target 検証の汎用処理のみ。具体ライブラリ名を一切持たない |
| `cmake/deps/<name>.cmake` | **各ライブラリの結線**。kazmath / curl など 1 ファイル 1 ライブラリ |
| `third_party/dependencies.cmake` | **レジストリ**。全依存の宣言 (pin) と、`ERGO_WITH_<NAME>` が ON のものだけを opt-in ビルド。傘 target `ergo_thirdparty` を公開 |

トップレベル `CMakeLists.txt` は gtest ブロック直後で
`third_party/dependencies.cmake` を `include` する。

## ビルドフラグ

| キー | 既定 | 役割 |
|---|---|---|
| `ERGO_FETCH_DEPENDENCIES` | ON | 依存を upstream から取得してよいかのマスタースイッチ。OFF かつ source override も無い状態で依存が要求されると **FATAL_ERROR** (無言で素通りしない) |
| `ERGO_WITH_KAZMATH` | OFF | kazmath を取得してビルド |
| `ERGO_WITH_CURL` | OFF | libcurl を取得してビルド |
| `ERGO_BUILD_THIRDPARTY_SMOKE` | OFF | スモークテストをビルド (kazmath + curl を強制 ON) |
| `ERGO_CURL_USE_SSL` | ON | curl の TLS backend を有効化 (Windows は Schannel = OS 内蔵、外部 SSL SDK 不要) |
| `ERGO_DEPENDENCY_CACHE_DIR` | (空) | 取得ソースの共有キャッシュ dir。空なら `<build>/_deps` |
| `ERGO_SOURCE_DIR_<UPPER>` | (空) | その依存を fetch せずローカルチェックアウトで差し替える (オフライン用) |

`WITH` 系を **既定 OFF** にしているのは、素の Ergo ビルドをネットワーク非依存・
軽量に保つため。必要なモジュール / ホストが明示的に ON にする。

## 取得済みライブラリ

| 名前 | pin | TLS / 備考 | consume |
|---|---|---|---|
| kazmath | commit `48dbc191…` (upstream に tag 無し) | C 数学ライブラリ。`GL/` ヘルパは除外し core .c だけで独自 `kazmath` target を組む (kazmath の CMake は `cmake_minimum_required(2.8)` で現行 CMake が拒否するため) | `target_link_libraries(<t> PRIVATE kazmath)` / `#include <kazmath/kazmath.h>` |
| curl | tag `curl-8_19_0` | static libcurl のみ (CLI/docs/test 無効)。Windows は Schannel | `target_link_libraries(<t> PRIVATE CURL::libcurl)` / `#include <curl/curl.h>` |

傘 target: `ergo_thirdparty` (= `ergo::thirdparty`) は ON になっている依存をまとめて
リンクする。どのスイッチが ON かを気にせず依存できる。

## 使い方

```bash
# kazmath + curl を取得して使う構成 + スモークテストで疎通確認
cmake -S . -B build -DERGO_BUILD_THIRDPARTY_SMOKE=ON
cmake --build build --config Debug --target test_thirdparty_deps
ctest --test-dir build -C Debug -R test_thirdparty_deps

# 個別に欲しい場合 (例: curl だけ)
cmake -S . -B build -DERGO_WITH_CURL=ON

# オフライン: 取得せずローカルソースを使う
cmake -S . -B build -DERGO_WITH_KAZMATH=ON -DERGO_SOURCE_DIR_KAZMATH=C:/src/kazmath
```

モジュール側からの利用例:

```cmake
# 依存が ON のときだけ結線する (OFF ビルドを壊さない)
if(TARGET kazmath)
    target_link_libraries(ergo_<mod> PRIVATE kazmath)
endif()
```

## 新しいライブラリを足す手順

1. `third_party/dependencies.cmake` に `ergo_declare_dependency(NAME <n> GIT_REPOSITORY … GIT_TAG <pin> LICENSE … )` を追加 (pin は full SHA か不変 tag)。
2. `cmake/deps/<n>.cmake` を作る:
   - upstream の CMake が新しく素直 → `ergo_require_dependency(<n> VERIFY_TARGETS <target>)`。option は呼ぶ前に `set(... CACHE BOOL "" FORCE)`。
   - upstream の CMake を走らせたくない (古い min / 余計な target) → `ergo_populate_dependency(<n> OUT_SOURCE_DIR src)` でソースだけ取得し、自前で `add_library` を組む。
3. `option(ERGO_WITH_<N> ... OFF)` を足し、ON のとき `include(deps/<n>)`。必要なら傘 target に追加。
4. `THIRD_PARTY_NOTICES.md` に attribution を追記。
5. (任意) スモーク/テストで疎通確認。

## 注意点

- **再現性**: `GIT_TAG` は必ず固定 (tag か full SHA)。`master`/`main` ブランチ名は禁止。
- **無言フォールバック禁止**: 取得不可・期待 target 不在は必ず FATAL_ERROR
  (`ergo_verify_targets`)。upstream のレイアウト変更を早期に検知する。
- **CRT 整合 (MSVC)**: kazmath は唯一の C target で、放置すると `cl.exe` 既定の
  `/MT` (静的 CRT) になり Ergo の動的 CRT C++ target と LNK4098 で衝突する。
  `cmake/deps/kazmath.cmake` で `MSVC_RUNTIME_LIBRARY` を動的 CRT に固定済み。
- **curl のビルド時間**: 初回 configure で curl をビルドするため数十秒かかる。
  `ERGO_DEPENDENCY_CACHE_DIR` を共有すると複数 build tree で再取得を避けられる。
