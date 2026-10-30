# synced-from AIFormat/cpp@1f027a3
# AIFormat/cpp/LudiarsCxxBaseline.cmake — LUDIARS C++ ビルド基盤の正本
#
# 規約の正本: AIFormat/RULE_CODE_CPP.md §C4 (ビルド基盤)。本ファイルは次を実装する。
#   §C4.1 標準 (C++20 下限、C++23 選択可) / §C4.2 baseline target 2 つと configure 検査 /
#   §C4.3 警告 / §C4.4 compile_commands の検査 / §C4.7 他リポ src の include 検査 /
#   §C1.7 modules 不採用 / §C2.8 未初期化の pattern 化 / §C2.9 標準ライブラリの hardening /
#   §C3.7 sanitizer / §C7.4 浮動小数の決定性。
# 版: LUDIARS_CXX_BASELINE_VERSION (下の値)。互換性を壊す変更で major を上げる。
# 同期: 各 C++ リポへは AIFormat/cpp/README.md の手順でコピーする (cmake/ 配下を推奨)。
#   コピー先の 1 行目は "# synced-from AIFormat/cpp@<commit>"。中身は書き換えない。
#   リポ固有の設定は include した側の CMakeLists.txt に書く (§C9.3 と同じ扱い)。
# 必要な CMake: 3.20 以上 (cmake_language(DEFER) と cxx_std_23 のため)。
#   C++ リポ自身の cmake_minimum_required は §C3.6 により 3.24 以上とする。
#   CMake 3.22 以上を最小にした includer では CMP0128 が NEW になり、コンパイラ既定の
#   標準が要求を満たす場合も拡張 OFF (-std=c++NN) が確実に付く。
#
# ---------------------------------------------------------------------------
# 使い方 (最小形)
#
#   cmake_minimum_required(VERSION 3.24)
#   project(ergo LANGUAGES CXX)                       # CXX を有効にしてから include する
#   include(cmake/LudiarsCxxBaseline.cmake)
#
#   add_library(ergo_math STATIC ...)
#   target_link_libraries(ergo_math
#     PUBLIC  ludiars_cxx_usage                        # §C4.2 (a) 必ず PUBLIC
#     PRIVATE ludiars_cxx_warnings)                    # §C4.2 (b) 必ず PRIVATE
#   ludiars_cxx_mark_warnings_compliant(ergo_math)     # §C4.3 準拠 target 一覧 (ラチェット)
#   ludiars_cxx_mark_logic_layer(ergo_world)           # §C7.2 / §C7.4 ロジック層の宣言
#   ludiars_cxx_check_targets(PREFIX ergo_ MODE WARNING)   # §C4.2 移行完了後は FATAL_ERROR
#
# キャッシュ変数 (preset で指定する。target ごとに書かない)
#   LUDIARS_CXX_STANDARD            20 | 23   (既定 20)        §C4.1
#   LUDIARS_CXX_WARNINGS_AS_ERRORS  ON | OFF  (既定 OFF)       §C4.3 verify preset で ON
#   LUDIARS_CXX_HARDENING           ON | OFF  (既定 OFF)       §C2.9 debug / test / sanitizer preset で ON
#   LUDIARS_CXX_SANITIZE            OFF | asan-ubsan | tsan    §C3.7 sanitizer preset で指定
#   LUDIARS_CXX_MSVC_CONFORMANCE    ON | OFF  (既定 OFF)       §C4.2 /permissive- /Zc:preprocessor の opt-in
#
# 関数
#   ludiars_cxx_enable_hardening(<target>)                                   §C2.9 / §C2.8
#   ludiars_cxx_enable_sanitizers(<target> [THREAD] [UNSIGNED_INTEGER_OVERFLOW] [IMPLICIT_CONVERSION])
#                                                                            §C3.7 / §C2.6 / §C2.7
#   ludiars_cxx_enable_strict_warnings(<target>)                             §C4.3 厳しい警告セット / §C2.6 境界層は必須
#   ludiars_cxx_mark_warnings_compliant(<target>...)                         §C4.3
#   ludiars_cxx_mark_logic_layer(<target>...)                                §C7.4
#   ludiars_cxx_check_targets(PREFIX <prefix> [MODE WARNING|FATAL_ERROR])    §C4.1 / §C4.2 / §C4.4 / §C4.7 / §C7.4
# ---------------------------------------------------------------------------

set(_LUDIARS_CXX_BASELINE_THIS_VERSION "1.0.0")

if(CMAKE_VERSION VERSION_LESS 3.20)
  message(FATAL_ERROR
    "LudiarsCxxBaseline: CMake 3.20 以上が必要 (現在 ${CMAKE_VERSION})。"
    "C++ リポの cmake_minimum_required は 3.24 以上とする (RULE_CODE_CPP §C3.6)。")
endif()

# include() は既定でポリシースコープを push するので、ここでの設定は includer に漏れない。
cmake_policy(VERSION 3.20...3.28)

# ---------------------------------------------------------------------------
# ディレクトリ既定値 — include した各ディレクトリに毎回適用する
# (target プロパティの初期値は INTERFACE target では伝播できないため変数で与える。
#  baseline は §C4.5 のディレクトリスコープ禁止の例外)
# ---------------------------------------------------------------------------
macro(_ludiars_cxx_apply_directory_defaults)
  # §C4.1 拡張は OFF (-std=c++NN。gnu++NN にしない)
  set(CMAKE_CXX_EXTENSIONS OFF)
  # §C1.7 modules は不採用。CMake 3.28+ の依存スキャン (clang-scan-deps 依存) を止める
  set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endmacro()

# ---------------------------------------------------------------------------
# include ガード — baseline target は同一ビルドで最上位の 1 回だけ定義して共用する
# (§C4.1: 標準は最上位の指定で決まる)。後から include された同期コピーの版が
# 定義済みの版より新しいときは、最上位のコピーが古いので fail-fast する (RULE_CODE §7.1)。
# ---------------------------------------------------------------------------
get_property(_ludiars_cxx_loaded_version GLOBAL PROPERTY LUDIARS_CXX_BASELINE_VERSION)
if(_ludiars_cxx_loaded_version)
  if(_ludiars_cxx_loaded_version VERSION_LESS _LUDIARS_CXX_BASELINE_THIS_VERSION)
    message(FATAL_ERROR
      "LudiarsCxxBaseline: 定義済みの baseline (${_ludiars_cxx_loaded_version}) が "
      "${CMAKE_CURRENT_LIST_FILE} (${_LUDIARS_CXX_BASELINE_THIS_VERSION}) より古い。"
      "最上位リポの同期コピーを AIFormat/cpp から更新する。")
  endif()
  _ludiars_cxx_apply_directory_defaults()
  unset(_ludiars_cxx_loaded_version)
  return()
endif()
unset(_ludiars_cxx_loaded_version)
set_property(GLOBAL PROPERTY LUDIARS_CXX_BASELINE_VERSION "${_LUDIARS_CXX_BASELINE_THIS_VERSION}")
set(LUDIARS_CXX_BASELINE_VERSION "${_LUDIARS_CXX_BASELINE_THIS_VERSION}")

# ---------------------------------------------------------------------------
# コンパイラ系統の判定 — 4 toolchain (MSVC / Apple clang / Android NDK clang / clang) と
# Linux の GCC だけを受け付ける。未知のコンパイラで黙って素通りしない。
# ---------------------------------------------------------------------------
if(NOT CMAKE_CXX_COMPILER_LOADED)
  message(FATAL_ERROR
    "LudiarsCxxBaseline: CXX 言語が有効になっていない。project(... LANGUAGES CXX) の後で include する。")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(_LUDIARS_CXX_FAMILY "msvc")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|AppleClang)$" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  set(_LUDIARS_CXX_FAMILY "clang-cl")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|AppleClang)$")
  set(_LUDIARS_CXX_FAMILY "clang")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(_LUDIARS_CXX_FAMILY "gcc")
else()
  message(FATAL_ERROR
    "LudiarsCxxBaseline: 未対応の C++ コンパイラ '${CMAKE_CXX_COMPILER_ID}'。"
    "対応は MSVC / clang-cl / Clang / AppleClang / GNU (RULE_CODE_CPP §C4)。")
endif()

# C / OBJC / OBJCXX にも CXX と同じ系統のフラグを付けるので、系統が食い違う構成は止める。
foreach(_ludiars_lang IN ITEMS C OBJC OBJCXX)
  if(CMAKE_${_ludiars_lang}_COMPILER_LOADED)
    set(_ludiars_lang_msvc_like FALSE)
    if(CMAKE_${_ludiars_lang}_COMPILER_ID STREQUAL "MSVC"
       OR CMAKE_${_ludiars_lang}_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
      set(_ludiars_lang_msvc_like TRUE)
    endif()
    if(_LUDIARS_CXX_FAMILY MATCHES "^(msvc|clang-cl)$")
      set(_ludiars_cxx_msvc_like TRUE)
    else()
      set(_ludiars_cxx_msvc_like FALSE)
    endif()
    if(NOT _ludiars_lang_msvc_like STREQUAL _ludiars_cxx_msvc_like)
      message(FATAL_ERROR
        "LudiarsCxxBaseline: ${_ludiars_lang} コンパイラ (${CMAKE_${_ludiars_lang}_COMPILER_ID}) と "
        "CXX コンパイラ (${CMAKE_CXX_COMPILER_ID}) のコマンドライン系統が異なる。同じ系統に揃える。")
    endif()
  endif()
endforeach()
unset(_ludiars_lang)
unset(_ludiars_lang_msvc_like)
unset(_ludiars_cxx_msvc_like)

# ---------------------------------------------------------------------------
# §C4.1 標準 — C++20 を下限とし、C++23 を選んでよい (許可であり義務ではない)
# ---------------------------------------------------------------------------
set(_LUDIARS_CXX_ALLOWED_STANDARDS 20 23)
if(NOT DEFINED LUDIARS_CXX_STANDARD)
  set(LUDIARS_CXX_STANDARD 20 CACHE STRING
    "LUDIARS の C++ 標準 (20 | 23)。RULE_CODE_CPP §C4.1。C++23 は §C1.7 の採否表で確認した範囲で選ぶ")
endif()
if(DEFINED CACHE{LUDIARS_CXX_STANDARD})
  set_property(CACHE LUDIARS_CXX_STANDARD PROPERTY STRINGS ${_LUDIARS_CXX_ALLOWED_STANDARDS})
endif()
if(NOT LUDIARS_CXX_STANDARD IN_LIST _LUDIARS_CXX_ALLOWED_STANDARDS)
  message(FATAL_ERROR
    "LudiarsCxxBaseline: LUDIARS_CXX_STANDARD='${LUDIARS_CXX_STANDARD}' は許可されていない。"
    "許可値は 20 / 23 (C++20 が下限。RULE_CODE_CPP §C4.1)。")
endif()

# ---------------------------------------------------------------------------
# 選択肢 (preset で指定する)
# ---------------------------------------------------------------------------
option(LUDIARS_CXX_WARNINGS_AS_ERRORS
  "§C4.3 警告のエラー化。verify preset で ON、add_subdirectory で取り込むホストの既定は OFF" OFF)
option(LUDIARS_CXX_HARDENING
  "§C2.9 標準ライブラリの hardening を ludiars_cxx_warnings 経由で付ける (debug / test / sanitizer preset)" OFF)
option(LUDIARS_CXX_MSVC_CONFORMANCE
  "§C4.2 MSVC の /permissive- /Zc:preprocessor を ludiars_cxx_usage に付ける (opt-in)" OFF)

set(_LUDIARS_CXX_ALLOWED_SANITIZE OFF asan-ubsan tsan)
if(NOT DEFINED LUDIARS_CXX_SANITIZE)
  set(LUDIARS_CXX_SANITIZE OFF CACHE STRING
    "§C3.7 sanitizer (OFF | asan-ubsan | tsan)。sanitizer preset で指定する")
endif()
if(DEFINED CACHE{LUDIARS_CXX_SANITIZE})
  set_property(CACHE LUDIARS_CXX_SANITIZE PROPERTY STRINGS ${_LUDIARS_CXX_ALLOWED_SANITIZE})
endif()
if(NOT LUDIARS_CXX_SANITIZE IN_LIST _LUDIARS_CXX_ALLOWED_SANITIZE)
  message(FATAL_ERROR
    "LudiarsCxxBaseline: LUDIARS_CXX_SANITIZE='${LUDIARS_CXX_SANITIZE}' は許可されていない。"
    "許可値は OFF / asan-ubsan / tsan (RULE_CODE_CPP §C3.7)。")
endif()

# 言語ごとの genex。C++ 専用の警告は CXX / OBJCXX にだけ付ける。
set(_LUDIARS_ALL_LANGS "C,CXX,OBJC,OBJCXX")
set(_LUDIARS_CXX_LANGS "CXX,OBJCXX")
# clang 専用フラグは「その TU を実際にコンパイルするコンパイラ」で判定する。
# CXX が clang でも C 側が gcc (cc) の構成では -Wthread-safety を gcc に渡して止まるため、
# CXX の系統ではなく COMPILE_LANG_AND_ID で言語ごとに見る。
set(_LUDIARS_CLANG_ALL "$<OR:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>,$<COMPILE_LANG_AND_ID:OBJC,Clang,AppleClang>,$<COMPILE_LANG_AND_ID:OBJCXX,Clang,AppleClang>>")
set(_LUDIARS_CLANG_CXX "$<OR:$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>,$<COMPILE_LANG_AND_ID:OBJCXX,Clang,AppleClang>>")

# 関数は別ディレクトリ (兄弟の add_subdirectory 等) からも呼ばれるので、判定結果は
# ディレクトリ変数ではなく GLOBAL プロパティに置き、各関数の先頭で読み込む。
set_property(GLOBAL PROPERTY _LUDIARS_CXX_FAMILY "${_LUDIARS_CXX_FAMILY}")
set_property(GLOBAL PROPERTY _LUDIARS_ALL_LANGS "${_LUDIARS_ALL_LANGS}")
set_property(GLOBAL PROPERTY _LUDIARS_CXX_LANGS "${_LUDIARS_CXX_LANGS}")
macro(_ludiars_cxx_load_globals)
  get_property(_LUDIARS_CXX_FAMILY GLOBAL PROPERTY _LUDIARS_CXX_FAMILY)
  get_property(_LUDIARS_ALL_LANGS GLOBAL PROPERTY _LUDIARS_ALL_LANGS)
  get_property(_LUDIARS_CXX_LANGS GLOBAL PROPERTY _LUDIARS_CXX_LANGS)
  get_property(_LUDIARS_CXX_TRIVIAL_INIT_FLAG GLOBAL PROPERTY _LUDIARS_CXX_TRIVIAL_INIT_FLAG)
endmacro()

# ===========================================================================
# §C4.2 (a) ludiars_cxx_usage — 全 target が PUBLIC でリンクする使用要件
#   consumer の TU にも必要なものだけを持つ: 標準、MSVC の文字コードと __cplusplus、
#   §C7.4 の fp フラグ。警告は入れない (§C4.3)。
# ===========================================================================
add_library(ludiars_cxx_usage INTERFACE)

# §C4.1 標準は compile feature で付与し、PUBLIC リンクで consumer へ伝播させる
target_compile_features(ludiars_cxx_usage INTERFACE cxx_std_${LUDIARS_CXX_STANDARD})

if(_LUDIARS_CXX_FAMILY STREQUAL "msvc" OR _LUDIARS_CXX_FAMILY STREQUAL "clang-cl")
  target_compile_options(ludiars_cxx_usage INTERFACE
    # §C4.2 ソースと実行文字集合を UTF-8 に固定 (非 ASCII を含む公開ヘッダが consumer TU でも化けない)
    "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:/utf-8>"
    # §C4.2 __cplusplus を実際の標準値にする (既定の MSVC は 199711L のまま)
    "$<$<COMPILE_LANGUAGE:${_LUDIARS_CXX_LANGS}>:/Zc:__cplusplus>"
    # §C7.4 (1) 浮動小数の決定性。/fp:contract は付けない (VS2022 の /fp:precise は縮約しない)
    "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:/fp:precise>")
  if(_LUDIARS_CXX_FAMILY STREQUAL "clang-cl")
    # §C7.4 (1) clang-cl の /fp:precise は文内の FMA 縮約を許すので明示的に止める
    target_compile_options(ludiars_cxx_usage INTERFACE
      "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:/clang:-ffp-contract=off>")
  endif()
  if(LUDIARS_CXX_MSVC_CONFORMANCE)
    # §C4.2 opt-in。/std:c++20 以上は /permissive- を既に含むが、consumer の /permissive を上書きする意図で明示する
    target_compile_options(ludiars_cxx_usage INTERFACE
      "$<$<COMPILE_LANGUAGE:${_LUDIARS_CXX_LANGS}>:/permissive->")
    if(_LUDIARS_CXX_FAMILY STREQUAL "msvc")
      # clang のプリプロセッサは既に規格準拠なので MSVC 本体だけに付ける
      target_compile_options(ludiars_cxx_usage INTERFACE
        "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:/Zc:preprocessor>")
    endif()
  endif()
else()
  # §C7.4 (1) 浮動小数の決定性。arm64 clang の既定 (fp-contract=on) で a += s * b が
  # fmadd に縮約され x64 と結果が分かれるのを防ぐ。inline ヘッダの演算を consumer の TU でも
  # 揃えるため usage (PUBLIC) に置く。
  target_compile_options(ludiars_cxx_usage INTERFACE
    "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:-ffp-contract=off>")
endif()

# ===========================================================================
# §C4.2 (b) ludiars_cxx_warnings — 全 target が PRIVATE でリンクする
#   警告 (§C4.3)、Windows SDK のマクロ衛生、§C2.9 hardening (option)、§C3.7 sanitizer (option)。
#   PUBLIC / INTERFACE で伝播させない (consumer のビルドを壊さないため)。
# ===========================================================================
add_library(ludiars_cxx_warnings INTERFACE)

# §C4.2 Windows SDK のマクロ衛生。ソース内の #define NOMINMAX は禁止し、ここに集める
target_compile_definitions(ludiars_cxx_warnings INTERFACE
  "$<$<PLATFORM_ID:Windows>:NOMINMAX>"
  "$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>")

# §C4.3 警告セット — clang / gcc と MSVC の対応表
#
#   目的 (規約)                    clang / gcc                    MSVC
#   -----------------------------  -----------------------------  ------------------------------
#   基本 (Praesidium と同じ)       -Wall -Wextra                  /W4
#   仮想 dtor 欠落 (§C1.5)         -Wnon-virtual-dtor             /w44265
#   return の欠落 (§C1.3)          -Wreturn-type (-Wall に含む)   C4715 (既定で有効)
#   書式文字列 (§C2.1 / §C8.4)     -Wformat=2                     /w44774 (既定で無効なので有効化)
#   符号比較 (§C2.7)               -Wsign-compare                 C4018 / C4389 (/W4 に含む)
#   enum の switch 漏れ            -Wswitch (-Wall に含む)        /w44062 (既定で無効なので有効化)
#   nodiscard 無視 (§C5.1)         -Wunused-result (既定で有効)   C4834 (既定で有効)
#   非推奨 API (§C2.13)            -Wdeprecated-declarations      C4996 (既定で有効)
#   スレッド安全性 (§C6.6)         clang: -Wthread-safety         — (マクロを空にする)
#   ダングリング (§C6.5)           clang: -Wdangling-gsl          —
#   指示付き初期化子順 (§C8.3)     clang: -Wreorder-init-list     C7560 (常にエラー)
#
#   clang-cl は MSVC 列に従い、clang 専用の行 (-Wthread-safety / -Wdangling-gsl /
#   -Wreorder-init-list とそのエラー化) も /clang: で付ける。
#   -Wpedantic は入れない (##__VA_ARGS__ で大量に出る。RULE_CODE_CPP「採らなかった論点」)。
#   -Wshadow -Wconversion -Wsign-conversion は ludiars_cxx_enable_strict_warnings で opt-in。
if(_LUDIARS_CXX_FAMILY STREQUAL "msvc" OR _LUDIARS_CXX_FAMILY STREQUAL "clang-cl")
  target_compile_options(ludiars_cxx_warnings INTERFACE
    "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:/W4>"
    "$<$<COMPILE_LANGUAGE:${_LUDIARS_CXX_LANGS}>:/w44265>"
    "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:/w44774>"
    "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:/w44062>")
  if(_LUDIARS_CXX_FAMILY STREQUAL "clang-cl")
    # §C6.6 / §C6.5 / §C8.3 clang-cl でも clang 専用の検査を抜かさない
    target_compile_options(ludiars_cxx_warnings INTERFACE
      "$<${_LUDIARS_CLANG_ALL}:/clang:-Wthread-safety>"
      "$<${_LUDIARS_CLANG_CXX}:/clang:-Wdangling-gsl;/clang:-Wreorder-init-list>")
  endif()
else()
  target_compile_options(ludiars_cxx_warnings INTERFACE
    "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:-Wall;-Wextra;-Wformat=2;-Wsign-compare>"
    "$<$<COMPILE_LANGUAGE:${_LUDIARS_CXX_LANGS}>:-Wnon-virtual-dtor>")
  if(_LUDIARS_CXX_FAMILY STREQUAL "clang")
    target_compile_options(ludiars_cxx_warnings INTERFACE
      "$<${_LUDIARS_CLANG_ALL}:-Wthread-safety>"
      "$<${_LUDIARS_CLANG_CXX}:-Wdangling-gsl;-Wreorder-init-list>")
  endif()
endif()

# §C4.3 エラー化 — LUDIARS_CXX_WARNINGS_AS_ERRORS に従う (個別の -Werror= を常時有効にすると
# 新しいコンパイラを使うホストのビルドを壊すため、ゲートは verify preset で担保する)。
#   段 1: 各 §C が要求する個別のエラー化。warnings をリンクした全 target に付く。
#   段 2: 全警告のエラー化。「準拠 target 一覧」(ludiars_cxx_mark_warnings_compliant) の
#         target にだけ付く (ラチェット)。consumer target のプロパティを genex で読む。
if(LUDIARS_CXX_WARNINGS_AS_ERRORS)
  if(_LUDIARS_CXX_FAMILY STREQUAL "msvc" OR _LUDIARS_CXX_FAMILY STREQUAL "clang-cl")
    target_compile_options(ludiars_cxx_warnings INTERFACE
      # §C1.3 return の欠落 / §C2.1 書式文字列 / §C5.1 nodiscard / §C2.13 非推奨 / §C1.5 仮想 dtor
      "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:/we4715;/we4774;/we4834;/we4996>"
      "$<$<COMPILE_LANGUAGE:${_LUDIARS_CXX_LANGS}>:/we4265;/we4062>"
      "$<$<AND:$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>,$<BOOL:$<TARGET_PROPERTY:LUDIARS_CXX_WARNINGS_COMPLIANT>>>:/WX>")
    if(_LUDIARS_CXX_FAMILY STREQUAL "clang-cl")
      target_compile_options(ludiars_cxx_warnings INTERFACE
        # §C6.6 スレッド安全性 / §C6.5 ダングリング / §C8.3 指示付き初期化子の順序
        "$<${_LUDIARS_CLANG_ALL}:/clang:-Werror=thread-safety>"
        "$<${_LUDIARS_CLANG_CXX}:/clang:-Werror=dangling-gsl;/clang:-Werror=reorder-init-list>")
    endif()
  else()
    target_compile_options(ludiars_cxx_warnings INTERFACE
      # §C1.3 return の欠落 / §C2.1 書式文字列 / §C5.1 nodiscard / §C2.13 非推奨
      "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:-Werror=return-type;-Werror=format-nonliteral;-Werror=format-security;-Werror=unused-result;-Werror=deprecated-declarations>"
      # §C1.5 仮想 dtor
      "$<$<COMPILE_LANGUAGE:${_LUDIARS_CXX_LANGS}>:-Werror=non-virtual-dtor>"
      "$<$<AND:$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>,$<BOOL:$<TARGET_PROPERTY:LUDIARS_CXX_WARNINGS_COMPLIANT>>>:-Werror>")
    if(_LUDIARS_CXX_FAMILY STREQUAL "clang")
      target_compile_options(ludiars_cxx_warnings INTERFACE
        # §C6.6 スレッド安全性 / §C6.5 ダングリング / §C8.3 指示付き初期化子の順序
        "$<${_LUDIARS_CLANG_ALL}:-Werror=thread-safety>"
        "$<${_LUDIARS_CLANG_CXX}:-Werror=dangling-gsl;-Werror=reorder-init-list>")
    endif()
  endif()
endif()

# ===========================================================================
# §C2.9 標準ライブラリの hardening / §C2.8 未初期化の pattern 化
#   _GLIBCXX_ASSERTIONS と _LIBCPP_HARDENING_MODE は ABI を変えないので、hardening 有無の
#   TU が混在してもリンクできる。MSVC は既定の _ITERATOR_DEBUG_LEVEL を上書きしない
#   (構成ごとの値が食い違うと LNK2038 になるため)。代わりに ABI を変えない
#   _MSVC_STL_HARDENING (VS 2022 17.14 以降で有効、それ以前は無視される) を付ける。
# ===========================================================================
set(_LUDIARS_CXX_TRIVIAL_INIT_FLAG "")
if(_LUDIARS_CXX_FAMILY STREQUAL "gcc" OR _LUDIARS_CXX_FAMILY STREQUAL "clang")
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag("-ftrivial-auto-var-init=pattern" LUDIARS_CXX_HAS_TRIVIAL_AUTO_VAR_INIT_PATTERN)
  if(LUDIARS_CXX_HAS_TRIVIAL_AUTO_VAR_INIT_PATTERN)
    set(_LUDIARS_CXX_TRIVIAL_INIT_FLAG "-ftrivial-auto-var-init=pattern")
  endif()
endif()
set_property(GLOBAL PROPERTY _LUDIARS_CXX_TRIVIAL_INIT_FLAG "${_LUDIARS_CXX_TRIVIAL_INIT_FLAG}")

# 内部: <target> に hardening を <scope> (PRIVATE / INTERFACE) で付ける
function(_ludiars_cxx_apply_hardening target scope)
  _ludiars_cxx_load_globals()
  target_compile_definitions(${target} ${scope}
    # §C2.9 libstdc++ (GCC / Linux clang)
    _GLIBCXX_ASSERTIONS
    # §C2.9 libc++ 18+ (Apple clang / NDK r27+)。libstdc++ と MSVC STL では無視される
    _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE
    # §C2.9 MSVC STL。_ITERATOR_DEBUG_LEVEL は触らない
    _MSVC_STL_HARDENING=1)
  if(_LUDIARS_CXX_TRIVIAL_INIT_FLAG)
    # §C2.8 初期化忘れを決まったバイトパターンにして再現可能にする (検出は sanitizer / テスト側)
    target_compile_options(${target} ${scope}
      "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:${_LUDIARS_CXX_TRIVIAL_INIT_FLAG}>")
  endif()
endfunction()

if(LUDIARS_CXX_HARDENING)
  _ludiars_cxx_apply_hardening(ludiars_cxx_warnings INTERFACE)
  if(NOT _LUDIARS_CXX_TRIVIAL_INIT_FLAG AND NOT _LUDIARS_CXX_FAMILY MATCHES "^(msvc|clang-cl)$")
    message(STATUS "LudiarsCxxBaseline: §C2.8 -ftrivial-auto-var-init=pattern はこのコンパイラで使えないため付けない")
  endif()
  message(STATUS "LudiarsCxxBaseline: §C2.9 hardening ON (ludiars_cxx_warnings 経由)")
endif()

# ===========================================================================
# §C3.7 sanitizer
#   clang / gcc: -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
#   MSVC: /fsanitize=address のみ (UB は検査しない。そのことを検証結果に書く)。/RTC・/ZI・
#         /INCREMENTAL は ASan と両立しないので、残っていれば configure で止める。
#   tsan は clang のホストでだけ実行する (MSVC に無い)。
# ===========================================================================

# 内部: 種別から compile / link のフラグを決める
#   kind = asan-ubsan | tsan、unsigned_overflow = TRUE/FALSE
function(_ludiars_cxx_sanitizer_flags kind unsigned_overflow implicit_conversion out_compile out_link)
  _ludiars_cxx_load_globals()
  set(compile_flags "")
  set(link_flags "")
  if(_LUDIARS_CXX_FAMILY STREQUAL "msvc" OR _LUDIARS_CXX_FAMILY STREQUAL "clang-cl")
    if(kind STREQUAL "tsan")
      message(FATAL_ERROR
        "LudiarsCxxBaseline: §C3.7 tsan は MSVC 系では使えない。clang のホストで tsan preset を実行する。")
    endif()
    if(unsigned_overflow)
      message(WARNING
        "LudiarsCxxBaseline: §C2.7 unsigned-integer-overflow は MSVC 系では検査されない (検証結果に未実施と書く)")
    endif()
    if(implicit_conversion)
      message(WARNING
        "LudiarsCxxBaseline: §C2.6 implicit-integer-truncation / sign-change は MSVC 系では検査されない (検証結果に未実施と書く)")
    endif()
    _ludiars_cxx_assert_msvc_asan_compatible()
    get_property(_warned GLOBAL PROPERTY _LUDIARS_CXX_MSVC_UBSAN_WARNED)
    if(NOT _warned)
      message(WARNING
        "LudiarsCxxBaseline: §C3.7 MSVC 系の sanitizer は /fsanitize=address のみで、UB は検査していない。"
        "検証結果にその旨を書く。")
      set_property(GLOBAL PROPERTY _LUDIARS_CXX_MSVC_UBSAN_WARNED TRUE)
    endif()
    list(APPEND compile_flags "/fsanitize=address")
    list(APPEND link_flags "/INCREMENTAL:NO")
  else()
    if(kind STREQUAL "tsan")
      list(APPEND compile_flags "-fsanitize=thread" "-fno-omit-frame-pointer")
      list(APPEND link_flags "-fsanitize=thread")
    else()
      list(APPEND compile_flags
        "-fsanitize=address,undefined" "-fno-sanitize-recover=all" "-fno-omit-frame-pointer")
      list(APPEND link_flags "-fsanitize=address,undefined")
    endif()
    if(unsigned_overflow)
      if(_LUDIARS_CXX_FAMILY STREQUAL "clang")
        # §C2.7 符号なし整数の wrap は UB ではないので、宣言した層にだけ付ける
        list(APPEND compile_flags "-fsanitize=unsigned-integer-overflow")
        list(APPEND link_flags "-fsanitize=unsigned-integer-overflow")
      else()
        message(WARNING
          "LudiarsCxxBaseline: §C2.7 unsigned-integer-overflow は clang でだけ検査できる。"
          "${CMAKE_CXX_COMPILER_ID} では未実施 (検証結果に書く)")
      endif()
    endif()
    if(implicit_conversion)
      if(_LUDIARS_CXX_FAMILY STREQUAL "clang")
        # §C2.6 暗黙の縮小・符号変換で値が変わったら報告する (UB ではないので境界層にだけ付ける)
        list(APPEND compile_flags "-fsanitize=implicit-integer-truncation,implicit-integer-sign-change")
        list(APPEND link_flags "-fsanitize=implicit-integer-truncation,implicit-integer-sign-change")
      else()
        message(WARNING
          "LudiarsCxxBaseline: §C2.6 implicit-integer-truncation / sign-change は clang でだけ検査できる。"
          "${CMAKE_CXX_COMPILER_ID} では未実施 (検証結果に書く)")
      endif()
    endif()
  endif()
  set(${out_compile} "${compile_flags}" PARENT_SCOPE)
  set(${out_link} "${link_flags}" PARENT_SCOPE)
endfunction()

# 内部: MSVC の ASan と両立しないフラグが呼び出し元スコープに残っていれば止める
function(_ludiars_cxx_assert_msvc_asan_compatible)
  set(configs ${CMAKE_CONFIGURATION_TYPES})
  if(CMAKE_BUILD_TYPE)
    list(APPEND configs ${CMAKE_BUILD_TYPE})
  endif()
  set(vars CMAKE_CXX_FLAGS CMAKE_C_FLAGS)
  foreach(cfg IN LISTS configs)
    string(TOUPPER "${cfg}" cfg_upper)
    list(APPEND vars CMAKE_CXX_FLAGS_${cfg_upper} CMAKE_C_FLAGS_${cfg_upper})
  endforeach()
  foreach(var IN LISTS vars)
    if("${${var}}" MATCHES "(^| )[/-](RTC[1csu]*|ZI)( |$)")
      message(FATAL_ERROR
        "LudiarsCxxBaseline: §C3.7 MSVC の /fsanitize=address は ${CMAKE_MATCH_2} と両立しない "
        "(${var}='${${var}}')。sanitizer preset で ${var} から外す "
        "(/ZI の代わりに CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=ProgramDatabase)。")
    endif()
  endforeach()
  if(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT MATCHES "EditAndContinue")
    message(FATAL_ERROR
      "LudiarsCxxBaseline: §C3.7 CMAKE_MSVC_DEBUG_INFORMATION_FORMAT が EditAndContinue (/ZI)。"
      "sanitizer preset では ProgramDatabase にする。")
  endif()
endfunction()

if(NOT LUDIARS_CXX_SANITIZE STREQUAL "OFF")
  _ludiars_cxx_sanitizer_flags("${LUDIARS_CXX_SANITIZE}" FALSE FALSE _ludiars_san_compile _ludiars_san_link)
  target_compile_options(ludiars_cxx_warnings INTERFACE
    "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:${_ludiars_san_compile}>")
  target_link_options(ludiars_cxx_warnings INTERFACE ${_ludiars_san_link})
  message(STATUS "LudiarsCxxBaseline: §C3.7 sanitizer=${LUDIARS_CXX_SANITIZE} (ludiars_cxx_warnings 経由)")
  unset(_ludiars_san_compile)
  unset(_ludiars_san_link)
endif()

# ===========================================================================
# 公開関数
# ===========================================================================

# 内部: 自リポで定義した通常の target か確かめる (IMPORTED / ALIAS は対象外)
function(_ludiars_cxx_require_own_target fn target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "LudiarsCxxBaseline: ${fn}: target '${target}' が無い。")
  endif()
  get_target_property(imported ${target} IMPORTED)
  get_target_property(aliased ${target} ALIASED_TARGET)
  if(imported OR aliased)
    message(FATAL_ERROR "LudiarsCxxBaseline: ${fn}: '${target}' は IMPORTED / ALIAS なので対象にできない。")
  endif()
endfunction()

# ludiars_cxx_enable_hardening(<target>)
#   §C2.9 / §C2.8。preset 全体で付けるなら LUDIARS_CXX_HARDENING=ON を使い、これは
#   特定の target (テスト実行ファイル等) だけに付けたいときに使う。PRIVATE で付ける。
function(ludiars_cxx_enable_hardening target)
  if(ARGC GREATER 1)
    message(FATAL_ERROR "LudiarsCxxBaseline: ludiars_cxx_enable_hardening は target を 1 つだけ取る (余分: ${ARGN})")
  endif()
  _ludiars_cxx_require_own_target(ludiars_cxx_enable_hardening ${target})
  get_target_property(type ${target} TYPE)
  if(type STREQUAL "INTERFACE_LIBRARY")
    _ludiars_cxx_apply_hardening(${target} INTERFACE)
  else()
    _ludiars_cxx_apply_hardening(${target} PRIVATE)
  endif()
endfunction()

# ludiars_cxx_enable_sanitizers(<target> [THREAD] [UNSIGNED_INTEGER_OVERFLOW] [IMPLICIT_CONVERSION])
#   §C3.7 既定は asan + ubsan (MSVC は asan のみ)。THREAD で tsan (clang / gcc のみ)。
#   UNSIGNED_INTEGER_OVERFLOW (§C2.7) と IMPLICIT_CONVERSION (§C2.6) は境界層の target と
#   その fuzz_corpus テストに付ける (clang のみ。他のコンパイラでは未実施を WARNING で出す)。
#   compile は PRIVATE、link はライブラリなら PUBLIC (最終リンクに runtime を届ける)。
function(ludiars_cxx_enable_sanitizers target)
  _ludiars_cxx_load_globals()
  cmake_parse_arguments(PARSE_ARGV 1 arg "THREAD;UNSIGNED_INTEGER_OVERFLOW;IMPLICIT_CONVERSION" "" "")
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "LudiarsCxxBaseline: ludiars_cxx_enable_sanitizers: 不明な引数 ${arg_UNPARSED_ARGUMENTS}")
  endif()
  _ludiars_cxx_require_own_target(ludiars_cxx_enable_sanitizers ${target})
  if(arg_THREAD)
    set(kind "tsan")
  else()
    set(kind "asan-ubsan")
  endif()
  # address と thread は同じバイナリに同居できない
  if(NOT LUDIARS_CXX_SANITIZE STREQUAL "OFF" AND NOT LUDIARS_CXX_SANITIZE STREQUAL kind)
    message(FATAL_ERROR
      "LudiarsCxxBaseline: ludiars_cxx_enable_sanitizers(${target} ${kind}) は "
      "LUDIARS_CXX_SANITIZE=${LUDIARS_CXX_SANITIZE} と両立しない。")
  endif()
  _ludiars_cxx_sanitizer_flags("${kind}" "${arg_UNSIGNED_INTEGER_OVERFLOW}" "${arg_IMPLICIT_CONVERSION}" compile_flags link_flags)
  get_target_property(type ${target} TYPE)
  if(type STREQUAL "INTERFACE_LIBRARY")
    target_compile_options(${target} INTERFACE "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:${compile_flags}>")
    target_link_options(${target} INTERFACE ${link_flags})
  elseif(type STREQUAL "EXECUTABLE")
    target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:${compile_flags}>")
    target_link_options(${target} PRIVATE ${link_flags})
  else()
    target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:${compile_flags}>")
    target_link_options(${target} PUBLIC ${link_flags})
  endif()
endfunction()

# ludiars_cxx_enable_strict_warnings(<target>)
#   §C4.3 厳しい警告セット (opt-in)。§C2.6 により境界層 (外部由来のバイト列をデコード・
#   エンコードする target) には必ず付ける。
function(ludiars_cxx_enable_strict_warnings target)
  _ludiars_cxx_load_globals()
  _ludiars_cxx_require_own_target(ludiars_cxx_enable_strict_warnings ${target})
  if(_LUDIARS_CXX_FAMILY STREQUAL "msvc" OR _LUDIARS_CXX_FAMILY STREQUAL "clang-cl")
    # C4242 / C4365: 縮小・符号の暗黙変換 (既定で無効)。shadow 系 (C4456-C4459) は /W4 に含む
    set(flags "/w44242" "/w44365")
  else()
    set(flags "-Wshadow" "-Wconversion" "-Wsign-conversion")
  endif()
  target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:${_LUDIARS_ALL_LANGS}>:${flags}>")
endfunction()

# ludiars_cxx_mark_warnings_compliant(<target>...)
#   §C4.3 準拠 target 一覧。LUDIARS_CXX_WARNINGS_AS_ERRORS=ON のとき全警告がエラーになる。
#   新規 target は最初から載せ、一覧から外すことは禁止 (ラチェット)。
function(ludiars_cxx_mark_warnings_compliant)
  foreach(target IN LISTS ARGN)
    _ludiars_cxx_require_own_target(ludiars_cxx_mark_warnings_compliant ${target})
    set_property(TARGET ${target} PROPERTY LUDIARS_CXX_WARNINGS_COMPLIANT TRUE)
  endforeach()
endfunction()

# ludiars_cxx_mark_logic_layer(<target>...)
#   §C7.2 / §C7.4 ロジック層 (リプレイやサーバ照合の対象) の宣言。
#   ludiars_cxx_check_targets がこの target の fast-math 系フラグを FATAL_ERROR にする。
function(ludiars_cxx_mark_logic_layer)
  foreach(target IN LISTS ARGN)
    _ludiars_cxx_require_own_target(ludiars_cxx_mark_logic_layer ${target})
    set_property(TARGET ${target} PROPERTY LUDIARS_CXX_LOGIC_LAYER TRUE)
  endforeach()
endfunction()

# ludiars_cxx_check_targets(PREFIX <prefix> [MODE WARNING|FATAL_ERROR])
#   §C4.2 呼び出したディレクトリの処理の最後 (cmake_language(DEFER)) に、配下で定義された
#   <prefix> で始まる target を検査する。MODE は移行期間中 WARNING、移行完了後 FATAL_ERROR。
#   §C7.4 のロジック層の fast-math だけは MODE によらず FATAL_ERROR。
#   リポのトップの CMakeLists.txt で 1 回呼ぶ (§C4.7 は呼んだディレクトリの外を「他リポ」とみなす)。
function(ludiars_cxx_check_targets)
  cmake_parse_arguments(PARSE_ARGV 0 arg "" "PREFIX;MODE" "")
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "LudiarsCxxBaseline: ludiars_cxx_check_targets: 不明な引数 ${arg_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT arg_PREFIX)
    message(FATAL_ERROR "LudiarsCxxBaseline: ludiars_cxx_check_targets: PREFIX (例: ergo_) が必要")
  endif()
  if(NOT arg_MODE)
    set(arg_MODE "WARNING")
  endif()
  if(NOT arg_MODE MATCHES "^(WARNING|FATAL_ERROR)$")
    message(FATAL_ERROR "LudiarsCxxBaseline: ludiars_cxx_check_targets: MODE は WARNING / FATAL_ERROR (指定: ${arg_MODE})")
  endif()
  # DEFER の引数は実行時に評価されるので、今の値を埋め込んだコードとして登録する
  cmake_language(EVAL CODE
    "cmake_language(DEFER DIRECTORY [[${CMAKE_CURRENT_SOURCE_DIR}]] CALL _ludiars_cxx_run_checks [[${arg_PREFIX}]] [[${arg_MODE}]] [[${CMAKE_CURRENT_SOURCE_DIR}]] [[${CMAKE_CURRENT_BINARY_DIR}]])")
endfunction()

# 内部: <dir> 配下の全 buildsystem target を集める
function(_ludiars_cxx_collect_targets dir out)
  get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
  get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
  foreach(sub IN LISTS subdirs)
    _ludiars_cxx_collect_targets("${sub}" sub_targets)
    list(APPEND targets ${sub_targets})
  endforeach()
  set(${out} "${targets}" PARENT_SCOPE)
endfunction()

# 内部: DEFER から呼ばれる検査本体
function(_ludiars_cxx_run_checks prefix mode root_source_dir root_binary_dir)
  _ludiars_cxx_collect_targets("${root_source_dir}" all_targets)
  set(violations "")
  set(fatal_violations "")
  string(LENGTH "${prefix}" prefix_len)
  set(fast_math_regex "(^|[ :>=])(-ffast-math|-Ofast|-funsafe-math-optimizations|-ffp-model=fast|[/-]fp:fast)([ >]|$)")

  foreach(target IN LISTS all_targets)
    string(SUBSTRING "${target}" 0 ${prefix_len} head)
    if(NOT head STREQUAL prefix)
      continue()
    endif()
    get_target_property(type ${target} TYPE)
    if(NOT type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY|EXECUTABLE|INTERFACE_LIBRARY)$")
      continue()
    endif()
    get_target_property(link_libs ${target} LINK_LIBRARIES)
    get_target_property(iface_libs ${target} INTERFACE_LINK_LIBRARIES)
    if(NOT link_libs)
      set(link_libs "")
    endif()
    if(NOT iface_libs)
      set(iface_libs "")
    endif()

    # §C4.2 (a) usage は PUBLIC (ライブラリは INTERFACE 側にも必要。実行ファイルはどちらでもよい)
    if(type STREQUAL "INTERFACE_LIBRARY")
      if(NOT "ludiars_cxx_usage" IN_LIST iface_libs)
        list(APPEND violations "${target}: ludiars_cxx_usage を INTERFACE でリンクしていない (§C4.2)")
      endif()
    elseif(type STREQUAL "EXECUTABLE")
      if(NOT "ludiars_cxx_usage" IN_LIST link_libs)
        list(APPEND violations "${target}: ludiars_cxx_usage をリンクしていない (§C4.2)")
      endif()
    else()
      if(NOT "ludiars_cxx_usage" IN_LIST link_libs OR NOT "ludiars_cxx_usage" IN_LIST iface_libs)
        list(APPEND violations "${target}: ludiars_cxx_usage を PUBLIC でリンクしていない (§C4.2)")
      endif()
    endif()

    # §C4.2 (b) warnings は PRIVATE (INTERFACE target はコンパイル単位を持たないので対象外)
    if(NOT type STREQUAL "INTERFACE_LIBRARY")
      if(NOT "ludiars_cxx_warnings" IN_LIST link_libs)
        list(APPEND violations "${target}: ludiars_cxx_warnings をリンクしていない (§C4.2)")
      endif()
      # static ライブラリの PRIVATE 依存は $<LINK_ONLY:...> で入るので、素の名前だけを違反とする
      if("ludiars_cxx_warnings" IN_LIST iface_libs)
        list(APPEND violations "${target}: ludiars_cxx_warnings を PUBLIC / INTERFACE で伝播させている (§C4.3)")
      endif()
    elseif("ludiars_cxx_warnings" IN_LIST iface_libs)
      list(APPEND violations "${target}: ludiars_cxx_warnings を INTERFACE で伝播させている (§C4.3)")
    endif()

    if(NOT type STREQUAL "INTERFACE_LIBRARY")
      # §C4.1 target ごとの標準指定は禁止 (consumer に伝播しない)
      get_target_property(std ${target} CXX_STANDARD)
      if(std)
        list(APPEND violations "${target}: CXX_STANDARD=${std} を target に書いている。標準は ludiars_cxx_usage で決める (§C4.1)")
      endif()
      # §C4.1 拡張 OFF
      get_target_property(ext ${target} CXX_EXTENSIONS)
      if(ext STREQUAL "ext-NOTFOUND" OR ext)
        list(APPEND violations "${target}: CXX_EXTENSIONS が OFF でない (§C4.1。baseline を include した後に target を作る)")
      endif()
      # §C4.7 他リポの非公開ヘッダ (src/) を include path に足さない
      get_target_property(inc_dirs ${target} INCLUDE_DIRECTORIES)
      if(inc_dirs)
        foreach(dir IN LISTS inc_dirs)
          if(dir MATCHES "^\\$<" OR NOT IS_ABSOLUTE "${dir}")
            continue()
          endif()
          cmake_path(SET dir_norm NORMALIZE "${dir}")
          string(FIND "${dir_norm}/" "${root_source_dir}/" in_src)
          string(FIND "${dir_norm}/" "${root_binary_dir}/" in_bin)
          if(NOT in_src EQUAL 0 AND NOT in_bin EQUAL 0 AND dir_norm MATCHES "/src(/|$)")
            list(APPEND violations "${target}: 他リポの非公開ヘッダ '${dir}' を include している (§C4.7)")
          endif()
        endforeach()
      endif()

      # §C7.4 ロジック層の fast-math は MODE によらず止める
      get_target_property(logic ${target} LUDIARS_CXX_LOGIC_LAYER)
      if(logic)
        get_target_property(opts ${target} COMPILE_OPTIONS)
        if(NOT opts)
          set(opts "")
        endif()
        string(REPLACE ";" " " opts "${opts}")
        set(flag_sources "COMPILE_OPTIONS=${opts}" "CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}")
        foreach(cfg IN LISTS CMAKE_CONFIGURATION_TYPES CMAKE_BUILD_TYPE)
          if(cfg)
            string(TOUPPER "${cfg}" cfg_upper)
            list(APPEND flag_sources "CMAKE_CXX_FLAGS_${cfg_upper}=${CMAKE_CXX_FLAGS_${cfg_upper}}")
          endif()
        endforeach()
        foreach(src IN LISTS flag_sources)
          if(src MATCHES "${fast_math_regex}")
            list(APPEND fatal_violations "${target}: ロジック層に ${CMAKE_MATCH_2} がある (${src})。§C7.4 で禁止")
          endif()
        endforeach()
      endif()
    endif()
  endforeach()

  # §C4.4 Ninja / Makefiles では compile_commands.json を出す (VS / Xcode ジェネレータは出せない)。
  # add_subdirectory で取り込まれたときはホストの設定なので検査しない。
  cmake_path(COMPARE "${root_source_dir}" EQUAL "${CMAKE_SOURCE_DIR}" is_top_level)
  if(is_top_level AND CMAKE_GENERATOR MATCHES "Ninja|Makefiles" AND NOT CMAKE_EXPORT_COMPILE_COMMANDS)
    list(APPEND violations "CMAKE_EXPORT_COMPILE_COMMANDS が ON でない。ninja-dev / verify preset で ON にする (§C4.4)")
  endif()

  if(fatal_violations)
    list(JOIN fatal_violations "\n  - " text)
    message(FATAL_ERROR "LudiarsCxxBaseline: 規約違反 (RULE_CODE_CPP)\n  - ${text}")
  endif()
  if(violations)
    list(JOIN violations "\n  - " text)
    message(${mode} "LudiarsCxxBaseline: 規約違反 (RULE_CODE_CPP、MODE=${mode})\n  - ${text}")
  endif()
endfunction()

# 最初の include でもディレクトリ既定値を適用する
_ludiars_cxx_apply_directory_defaults()
