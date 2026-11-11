# Lorica の守り (ビルドの守りと入力の上限)

## 目的

Ergo が自分でコンパイルするビルド単位 (ライブラリ・実行ファイル・テスト・ツール・ベンチマーク・例) の全部に
LUDIARS の防御ライブラリ **Lorica** (`LUDIARS/Lorica`) を入れる。あわせて、外から入る入力
(ネットワークの受け口と、信頼できないファイルを読む parser) に上限の検査と境界テストを置く。
Lorica は守りの層を足すもので、メモリ安全を保証するものではない。

棚卸し・適用の状態・残りは [../plan/2026-11-11-lorica-adoption.md](../plan/2026-11-11-lorica-adoption.md)。

## 何が入るか

| 部品 | 中身 | 入れ方 |
|---|---|---|
| `Lorica::boundary` | `lorica/checked_size.hpp` (桁あふれを検査する足し算・掛け算、範囲の検査)、`byte_view.hpp`、`input_budget.hpp` (1 リクエストの累積の上限)。C++20 のヘッダ (`checked_integer` / `bounded_text` / `byte_reader`) は C++20 のターゲットだけ | 全ターゲットに `PRIVATE $<BUILD_INTERFACE:Lorica::boundary>` |
| baseline | Linux: `-fstack-protector-strong`、PIC / PIE、`-z relro -z now -z noexecstack`、最適化ありの構成で `_FORTIFY_SOURCE=3` (使えなければ 2)、`_GLIBCXX_ASSERTIONS` / libc++ FAST、`-fstack-clash-protection`、`-fcf-protection=full`。Windows: `/GS /sdl /guard:cf`、`/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA` | 全ターゲットに `lorica_harden_target` |
| EXTENDED | baseline + `-ftrivial-auto-var-init=zero`・libc++ EXTENSIVE・FORTIFY 3 の必須化 (Windows は STL hardening と `/CETCOMPAT`) | `ergo_hardening_mark_network()` で印を付けたターゲット (下表) |
| 危険な C 関数の拒否 | `strcpy` / `strcat` / `sprintf` / `vsprintf` / `wcscpy` / `wcscat` の使用をコンパイルエラーにする。**ターゲットの中の全ての非推奨 (`[[deprecated]]`) もエラーになる** | C のソースを持たない全ターゲットに `lorica_deprecate_unsafe_apis` |

EXTENDED の対象 (外から入力を受け続けるもの。毎フレームの描画・シミュレーションを持つライブラリは baseline のまま):
`ergo_common_ws`・`ergo_http`・`ergo_custos`・`ergo_studio_net`・`ergo_studio_host`・`ergo_studio_platform`・
`ergo_studio_plugins`・`ergo_studio` (実行ファイル)。

## 仕組み (`cmake/ErgoHardening.cmake`)

1. `include(ErgoHardening)` (CMakeLists.txt の先頭近く): オプションと `ergo_hardening_mark_network()`。
2. `ergo_hardening_setup()` (パッケージ・ハブの後): このツールチェインで Lorica が使えるかを決め、Lorica を取り込む。
   - ホストがすでに `Lorica::boundary` と `lorica_harden_target` を持っていれば、それを使う (二重に足さない)。
   - 無ければ `LORICA_SOURCE_DIR` (キャッシュ、初回は同名の環境変数が既定値) → `ERGO_SOURCE_DIR_LORICA` →
     固定 commit の取得 (`third_party/dependencies.cmake`、`ERGO_FETCH_DEPENDENCIES=OFF` なら構成エラー)。
   - Lorica の `LORICA_BUILD_PROTECTION` / `LORICA_BUILD_TESTS` / `LORICA_BUILD_FUZZER` は関数スコープの通常変数で OFF
     (ホストのキャッシュに書かない)。
3. `ergo_harden_targets()` (CMakeLists.txt の最後の文): Ergo のソースツリーの中のディレクトリ (取得物の置き場の
   ビルドツリーは除く) の全ターゲットに上の 4 つを入れる。ネットワークの入れ子の構成をするテストには
   `LORICA_SOURCE_DIR` を環境で渡す (オフラインのまま)。
4. 最上位のディレクトリ (サブプロジェクトならホストの) の終わりに `ergo_hardening_verify()` が走り、守りの無い
   Ergo のターゲット (`LORICA_HARDENED` が無い、または危険な C 関数の拒否も理由の記録も無い) があれば構成エラー。

## Lorica が対応しない環境

Lorica の守りの設定は Windows の MSVC 系と Linux の GCC / Clang だけで、CMake 3.24 以上が要る。
macOS (AppleClang / Xcode)、iOS、Android (NDK)、Emscripten、MinGW、CMake 3.20〜3.23 では:

- `ERGO_HARDENING=auto` (既定): `ergo: Ergo's targets are built WITHOUT Lorica hardening: <理由>` の WARNING を出し、
  Lorica を取り込まずにビルドする (黙って外さない)。
- `ERGO_HARDENING=required`: 構成エラー。

危険な C 関数の拒否は、Lorica が clang-cl・MinGW・glibc 2.40 以上の Clang を断るので、そこでは
`ergo: unsafe C API rejection not applied with this toolchain: <理由>` を STATUS に出して baseline だけを入れる。

## ホストに取り込まれたとき

- ホストが先に Lorica を `add_subdirectory` / FetchContent / `find_package(Lorica CONFIG)` していれば、それを使う。
  Ergo の後でホストが Lorica を `add_subdirectory` すると `lorica_boundary` の二重定義になるので、ホストは
  `if(NOT TARGET Lorica::boundary)` で囲む。
- 守りはすべて Ergo のターゲットの `PRIVATE`。`Lorica::boundary` は `$<BUILD_INTERFACE:...>` なので、
  Ergo を静的にリンクするホストに伝わるのはリンクの順番だけ (インクルードパスも定義も伝わらない)。
- Lorica は守りを入れたターゲットを PIC にする (`ERGO_POSITION_INDEPENDENT_CODE=OFF` は効かず、WARNING を出す)。

## 入力の上限

| 経路 | 上限 | 場所 |
|---|---|---|
| Studio の HTTP | ヘッダ 64 KiB・ヘッダ行 + クエリの数 2000 (`input_budget`)・本文 32 MiB・Content-Length は 10 進の数字だけ (`checked_*`) | `include/ergo/studio/net/http_parser.h` |
| Studio の WebSocket | 1 メッセージ 32 MiB (断片の合計も)、長さの和は `checked_add` | `include/ergo/studio/net/ws_frame.h` |
| Studio の同時接続 | 64 (超えると 503) | `ServerConfig::max_connections` |
| Custos ブリッジ | ヘッダ 16 KiB (超えると 431)・本文 64 KiB (超えると 413)・Content-Length は 10 進の数字だけ・食い違う重複は 400 | `src/custos/request_head.h` |
| WS クライアント | 1 フレーム 1 MiB (既存) + 長さの和を `checked_add` | `src/common/ws_client.cpp` |
| JSON (`ergo::common::jsonm`) | 入力 64 MiB・値 1,048,576 個・深さ 64 (`input_budget`)、`\u` は 16 進 4 桁だけ | `include/ergo/common/json_min.h` (`ParseLimits`) |
| SVG のラスタ化 | 入力 16 MiB・1 辺 16384・ビットマップ 256 MiB (`checked_multiply`) | `include/ergo/ui/svg_raster.h` (`SvgRasterOptions`) |
| SVG のパス | 入力 16 MiB・点 1,048,576 個・図形 65,536 個 (`input_budget`)。`std::regex` をやめた (長い属性でスタックが溢れた) | `include/ergo/vector/vector.h` (`SvgLimits`) |
| Unity Shuriken の移行 | ファイル 512 MiB・GameObject の親子の深さ 1024 (`input_budget`) | `include/ergo/shuriken_migrator/migrator.h` |
| WAV | チャンネル 1〜64・サンプルレート 1〜768000 Hz・float は 32 bit だけ、読む量は `checked_multiply` | `include/ergo/sound/types.h` |

値の根拠は各ヘッダのコメントと [../plan/2026-11-11-lorica-adoption.md](../plan/2026-11-11-lorica-adoption.md)。

## sanitizer と fuzz

```bash
# ASan + UBSan (既存の CI のレグと同じ)
cmake --preset ci-asan -DLORICA_SOURCE_DIR=<lorica> && cmake --build --preset ci-asan && ctest --preset ci-asan
# ThreadSanitizer (手元)
cmake --preset ci-tsan -DLORICA_SOURCE_DIR=<lorica> && cmake --build --preset ci-tsan && ctest --preset ci-tsan
# libFuzzer (Clang)。入口ごとに 30 秒ほど
cmake --preset fuzz-clang -DLORICA_SOURCE_DIR=<lorica> && cmake --build --preset fuzz-clang
mkdir -p /tmp/corpus && cp -r tests/studio/fuzz_corpus/studio_net /tmp/corpus/
build/fuzz-clang/fuzz_studio_net -max_total_time=30 /tmp/corpus/studio_net
```

入口 (`tests/<module>/fuzz_*.cpp`): `studio_net` (HTTP + WS)、`json_min`、`custos_request_head`、`vector_svg`、
`shuriken_prefab`。同じ入口を種 (`tests/<module>/fuzz_corpus/<name>/`) と端の入力で 1 回ずつ呼ぶ
`fuzz_regress_<name>` は、どのコンパイラでも `ERGO_BUILD_TESTS` で作られ ctest に入る。

## Lorica が掛かっているかのテスト (ctest、`cmake/ErgoHardeningChecks.cmake`)

構成時の網羅の確認 (`ergo_hardening_verify`) はターゲットのプロパティしか見ない。ビルドの結果そのものを見る次の
テストが ctest に入る (ラベル `hardening`)。取り込みが外れた・守りのフラグが落ちた・強制 include が効いていない、
のどれでも ctest が落ちる。

| テスト | 中身 |
|---|---|
| `test_lorica_applied` | `tests/hardening/test_lorica_applied.cpp` を他のターゲットと同じ `ergo_harden_targets()` の歩き方で作り、翻訳単位の中から確かめる: `<lorica/unsafe_api.hpp>` の強制 include (このヘッダは `#pragma once` で印のマクロを残さないので、C ライブラリのヘッダより前に `::strcpy` が宣言済みであることで見る)、`_GLIBCXX_ASSERTIONS` / libc++ の `_LIBCPP_HARDENING_MODE`、最適化ありの構成で `_FORTIFY_SOURCE` が 3 (コンパイラ 12 未満は 2)、`lorica::checked_add` の桁あふれが `nullopt`、`lorica::input_budget` が上限ちょうどを通し +1 を断る |
| `test_lorica_applied_extended` | 同じソースを `ergo_hardening_mark_network()` で EXTENDED にしたもの。MSVC は `_MSVC_STL_HARDENING` / `_MSVC_STL_DESTRUCTOR_TOMBSTONES`、libc++ は EXTENSIVE。libstdc++ の EXTENDED (`-ftrivial-auto-var-init=zero`) は印のマクロが無いので次のテストで見る。EXTENDED が使えないツールチェインでは skip |
| `test_lorica_compile_commands` | `tests/hardening/test_lorica_compile_commands.cmake` (`cmake -P`)。`compile_commands.json` (Ergo が最上位のときだけ `CMAKE_EXPORT_COMPILE_COMMANDS=ON`) の Ergo 自身のソース (ビルドツリーの取得物は除く) の全エントリに、Lorica の include の場所、`-fstack-protector-strong`・`-fstack-clash-protection`・`-fcf-protection=full` (AArch64 は `-mbranch-protection=standard`)・PIC/PIE、C++ には `-D_GLIBCXX_ASSERTIONS`・`-D_LIBCPP_HARDENING_MODE=…`・`-include …/unsafe_api.hpp`・`-Werror=deprecated-declarations`、最適化ありの構成で最後の `_FORTIFY_SOURCE` が `-D_FORTIFY_SOURCE=3`、EXTENDED のターゲットには `-ftrivial-auto-var-init=zero` があり、打ち消すフラグ (`-fno-stack-protector`・`-D_FORTIFY_SOURCE=0`・`-fcf-protection=none`・`-Wno-deprecated-declarations` など) が無いことを見る。欠けたエントリはファイル名とターゲット名を挙げて落ちる。C のソースは C に付くフラグだけ |
| `test_lorica_verify_binary_<target>` | `python3` が見つかるときだけ登録 (無ければ configure の STATUS)。Lorica の `tools/verify_binary.py` を `ergo_studio`・`ergo_pkg_cli`・`test_lorica_applied` に掛ける (ELF: PIE・NX・full RELRO、PE: ASLR・NX・CFG) |

skip になるとき (ctest は「通過」ではなく skip と数える): Lorica が掛からない構成 (macOS・CMake 3.20〜3.23 など。
上の WARNING の構成) では全部、Visual Studio ジェネレータ (`compile_commands.json` を作らない) と MSVC では
`test_lorica_compile_commands`。MSVC の baseline (`/GS /sdl /guard:cf`・`/we4996`) は翻訳単位の中から見える印が無いので、
Windows では構成時の確認と `verify_binary.py` (PE のフラグ) が受け持つ。

## CI での Lorica の取得

`LUDIARS/Lorica` は private なので、CI は org の secret **`LUDIARS_REPO_TOKEN`** (repo:read 相当の PAT。LUDIARS の
private リポを CI で読むときの決まり) で取得する。`ci.yml` の `build-and-test` (windows-msvc・linux-clang・linux-asan)
と `packages-integration.yml` の `ci-packages` で:

1. `Lorica pin`: `third_party/dependencies.cmake` の Lorica の `GIT_TAG` (40 桁) を読む (SHA を二重に書かない)。読めなければ失敗。
2. `Require LUDIARS_REPO_TOKEN`: secret が空 (未設定、または fork からの PR) なら `::error::` で失敗。守りなしで続けない。
3. `Checkout Lorica`: `actions/checkout` で `build/_lorica` に、その commit を。
4. 構成に `-DLORICA_SOURCE_DIR=<その場所> -DERGO_HARDENING=required`。

`linux-gcc-cmake320` (CMake 3.20) と macOS は Lorica が対応しないので取得しない (WARNING で守りなし、上のテストは skip)。

## 守りの確かめ方

```bash
# 守りのフラグが入っていて、打ち消すものが無いこと
python3 -c "import json;[print(e['file']) for e in json.load(open('build/compile_commands.json')) if '-fstack-protector-strong' not in e['command']]"
grep -E -- "-fno-stack-protector|-D_FORTIFY_SOURCE=0|execstack|-U_FORTIFY_SOURCE *$" build/compile_commands.json
# 実行ファイルの PIE / NX / RELRO (Lorica の検査道具。実行はしない)
python3 -I <lorica>/tools/verify_binary.py --readelf /usr/bin/readelf build/studio/ergo_studio
```
