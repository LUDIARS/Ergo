#!/usr/bin/env node
/**
 * RULE_CODE_CPP §C9.5 の規則表 (正本: AIFormat/cpp/rules/cpp-rules.mjs)。
 *
 * - 2 つの経路が同じ表を使う。
 *   - hook: Castra/.claude/hooks/cpp-convention-check.mjs が、編集したファイル 1 本の追加行に
 *     `scanSource` (と `scanChangeset([その 1 本])`) を掛ける。severity "error" は block、"warn" は表示のみ。
 *   - gate: Revisor 登録テスト `cpp-build-policy` が CLI (`node cpp-rules.mjs <repo> --base <ref>`) で
 *     merge-base からの diff に掛け、warn 以上を fail にする (exit 1)。
 * - 正規表現による構文 tripwire であり、パーサではない。コメントと文字列リテラルは
 *   `maskCommentsAndLiterals` で空白化してから照合する (改行は残すので行番号は変わらない)。
 * - finding は §C 番号・規則名・ファイル・行番号・重大度・固定文言だけを持ち、ソース抜粋を入れない
 *   (Revisor の leakage と同じ方針)。
 * - このファイルの責務は「規則表」と、表を束ねた公開 API。字句処理・設定・実行・git は `lib/` に分ける。
 *   同期コピーを置くときは `cpp/rules/` を (テストを除いて) ディレクトリごとコピーする。
 * - 依存なし (Node 22 の標準モジュールのみ)。
 */
import path from "node:path";
import { fileURLToPath } from "node:url";

import { checkConceptTests, checkFuzzHarness, checkShaderLayoutSync } from "./lib/checks-changeset.mjs";
import { checkCacheForce, checkForeignSrc, checkPerTargetUtf8, checkPublicMathDefines } from "./lib/checks-cmake.mjs";
import {
  checkCStyleCast, checkFunctionalCast, checkGpuStructLayout, checkInterfaceDataMember, checkModuleUnitFile,
  checkNolintForm, checkPlatformIfCount, checkPositionalConfig, checkPragmaDisable, checkReinterpretCast,
  checkRemoveBeforeRename, checkSingleton, checkThreadSafetyNote, checkUnconstrainedTemplate, checkViewMember,
  releaseResultUsed,
} from "./lib/checks-cpp.mjs";
import { runCli } from "./lib/cli.mjs";
import { CONFIG_FILE, inAny, loadRepoConfig as loadConfig, parseRepoConfig as parseConfig } from "./lib/config.mjs";
import { scanChangeset as runChangeset, scanSource as runSource } from "./lib/engine.mjs";
import {
  BASELINE_CMAKE, CMAKE_SCRIPT_OR_BASELINE, OS_MACRO, OUTPUT_EXEMPT_DIRS, PATHS, PLATFORM_SPLIT_FILE,
} from "./lib/paths.mjs";

export { maskCommentsAndLiterals } from "./lib/mask.mjs";
export { languageOf, normalizePath, PATHS } from "./lib/paths.mjs";
export { collectChanges, parseUnifiedDiff } from "./lib/git-diff.mjs";
export { formatFinding } from "./lib/cli.mjs";
export { CONFIG_FILE };

/**
 * 規則表。1 エントリ = 1 検査。同じ §C に複数の検査があるときは `name` で区別する。
 *
 * - id: RULE_CODE_CPP の §C 番号 (安定参照 ID)。
 * - name: 規則名。抑制の check 名は `cpp-rules-<id>` (§C 単位)。
 * - severity: "error" = hook で block・gate で fail / "warn" = hook は表示のみ・gate で fail。
 * - files: 適用対象のパス。exclude: 除外するパス。skip(file, config): 設定による除外。
 * - when(config): 設定が揃ったときだけ有効にする (ロジック層・ロガーの宣言など)。
 * - scope: "added-lines" = finding の行 (anyOf のどれか) が追加行のときだけ報告 /
 *          "file" = 変更されたファイル全体で判定 / "changeset" = 変更集合全体で判定 (check(cs))。
 * - mask: 照合するテキスト。"code" (既定: コメントとリテラルを空白化) /
 *         "comments" (リテラルだけ空白化。注釈を見る規則) / "none" (原文)。
 * - pattern: 照合する正規表現 (名前付きグループ `at` があればその位置の行を報告)。
 * - filter(match, ctx): pattern の一致を絞る。check(ctx): pattern で書けない規則。
 * - suppressible: false なら `NOLINT(cpp-rules-<id>)` で抑制できない (コードを直す)。
 */
export const rules = [
  // ---- C1 Concept と型による契約 ----
  {
    id: "C1.1", name: "unconstrained-callable-template", severity: "warn",
    title: "制約のないテンプレート引数",
    files: PATHS.cpp, scope: "added-lines", check: checkUnconstrainedTemplate,
    message: "呼び出す型引数 (Fn/F/Func/Callback/Visitor 等) か Args&&... を転送する関数テンプレートに制約がない。std::invocable / std::constructible_from / 名前付き concept で制約する",
    why: "契約がコメントだけだと、誤った型でインスタンス化したときの診断が本体の奥で出る。concept にすると宣言で落ちる",
  },
  {
    id: "C1.2", name: "sfinae", severity: "warn",
    title: "SFINAE の禁止",
    files: PATHS.cpp, scope: "added-lines",
    pattern: /\b(?:std::)?(?:enable_if(?:_t)?|void_t)\b/,
    message: "enable_if / void_t による SFINAE は使わない。requires 節・標準 concept・if constexpr (Concept<T>) で書く",
    why: "SFINAE は失敗理由が読めず、AI も人も意図を取り違える。C++20 では concept で同じことが書ける",
  },
  {
    id: "C1.4", name: "concept-outside-concepts-h", severity: "warn",
    title: "concept の置き場所",
    files: PATHS.cpp, exclude: /(?:^|\/)concepts\.h$|(?:^|\/)tests?\//, scope: "added-lines",
    pattern: /\bconcept\s+[A-Za-z_]\w*\s*=/,
    message: "concept の定義はモジュールの公開ヘッダ直下の concepts.h にだけ書く (共有するものは下層の共通モジュールへ)",
    why: "concept が散ると同じ契約が重複し、モジュール間の cross-import を誘う",
  },
  {
    id: "C1.5", name: "interface-data-member", severity: "warn",
    title: "I<Name> はデータメンバを持たない",
    files: PATHS.cpp, scope: "added-lines", check: checkInterfaceDataMember,
    message: "I<Name> インターフェースに非 static データメンバがある。状態は実装クラスへ移す",
    why: "実行時境界の interface に状態があると ABI と寿命の契約が壊れ、差し替えの意味がなくなる",
  },
  {
    id: "C1.6", name: "ptr-size-param", severity: "warn",
    title: "(ptr, size) 引数",
    files: PATHS.publicHeader, scope: "added-lines",
    pattern: /(?<=[(,]\s*)const\s+(?:unsigned\s+)?[\w:]+\s*\*\s*[A-Za-z_]\w*\s*,\s*(?:std::)?size_t\b/,
    message: "読み取り専用の (const T* data, size_t size) 引数は std::span<const T> / std::string_view で受ける",
    why: "ポインタと長さを別々に渡すと、長さの取り違えが範囲外読み出しになる",
  },
  {
    id: "C1.6", name: "vector-param", severity: "warn",
    title: "const std::vector& 引数",
    files: PATHS.publicHeader, scope: "added-lines",
    pattern: /(?<=[(,]\s*)const\s+std::vector\s*</,
    message: "所有しない読み取り専用の引数は const std::vector<T>& ではなく std::span<const T> で受ける",
    why: "vector を要求すると呼び出し側に確保とコピーを強い、配列や部分範囲を渡せない",
  },
  {
    id: "C1.7", name: "modules-coroutines", severity: "error",
    title: "modules と coroutine を新規に導入しない",
    files: PATHS.cpp, scope: "added-lines",
    pattern: /^[ \t]*(?:export[ \t]+)?module[ \t]*(?:;|[A-Za-z_][\w.]*[ \t]*(?::[\w.]+)?[ \t]*;)|^[ \t]*(?:export[ \t]+)?import[ \t]+(?:<[^>\n]*>|[A-Za-z_:][\w.:]*|)[ \t]*;|\bco_(?:await|return|yield)\b/m,
    message: "C++20 modules と coroutine は採否表で不採用。ヘッダと通常の関数で書く",
    why: "Praesidium は翻訳単位ごとに clang --analyze するので BMI 前提の import を解析できず、MSVC/Xcode/NDK 間で BMI に互換性がない",
    suppressible: false,
  },
  {
    id: "C1.7", name: "module-unit-file", severity: "error",
    title: "モジュールのインターフェース単位",
    files: PATHS.moduleUnit, scope: "file", check: checkModuleUnitFile,
    message: "モジュールのインターフェース単位 (.ixx/.cppm/.mpp) は追加しない",
    why: "C++20 modules は採否表で不採用",
    suppressible: false,
  },
  {
    id: "C1.7", name: "cmake-modules", severity: "error",
    title: "CMake でモジュールを有効にしない",
    files: PATHS.cmake, scope: "added-lines",
    pattern: /CXX_SCAN_FOR_MODULES\s+(?:ON|TRUE|YES|1)\b|\bTYPE\s+CXX_MODULES\b/i,
    message: "CMAKE_CXX_SCAN_FOR_MODULES は OFF のまま、FILE_SET CXX_MODULES は使わない",
    why: "C++20 modules は採否表で不採用",
    suppressible: false,
  },
  {
    id: "C1.8", name: "load-library", severity: "error",
    title: "LoadLibrary(A|W) の禁止",
    files: PATHS.native, scope: "added-lines",
    pattern: /\bLoadLibrary[AW]?\s*\(/,
    message: "LoadLibrary(A|W) は使わない。絶対パスに解決して LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32) を *_loader.cpp で呼ぶ",
    why: "検索パス任せのロードは DLL planting の入口になる",
    suppressible: false,
  },
  {
    id: "C1.8", name: "loader-outside", severity: "warn",
    title: "実行時ロードは *_loader.cpp に閉じる",
    files: PATHS.native, exclude: /_loader\.(?:cpp|cc|cxx|mm)$/, scope: "added-lines",
    pattern: /\b(?:dlopen|LoadLibraryEx[AW]?)\s*\(/,
    message: "dlopen / LoadLibraryExW は <lib>_loader.cpp の中だけで呼ぶ (版の検査と関数表への解決を 1 箇所にまとめる)",
    why: "ロードの経路が散ると、相対パスへの縮退や版の不一致を見落とす",
  },

  // ---- C2 Revisor の脆弱性対応を取り込む ----
  {
    id: "C2.1", name: "banned-api", severity: "error",
    title: "禁止 API",
    files: PATHS.native, scope: "added-lines",
    pattern: /(?:\bstd::|(?<![\w.:]|->))(?:gets|strcpy|strcat|sprintf|vsprintf|wcscpy|wcscat|strncpy|strtok|scanf|sscanf|fscanf|atoi|atol|atoll|atof|alloca|_alloca|rand|srand)\s*\(/,
    message: "禁止 API (Praesidium の 7 関数、strncpy/strtok/scanf 系/atoi 系/alloca/rand) は使わない。std::string・std::from_chars・注入した RNG で書く",
    why: "境界や失敗を表せない API は、外部入力でバッファ溢れや無言の 0 を生む",
    suppressible: false,
  },
  {
    id: "C2.2", name: "std-sto", severity: "error",
    title: "std::sto* の禁止",
    files: PATHS.cpp, scope: "added-lines",
    pattern: /(?:\bstd::|(?<![\w.:]|->))sto(?:i|l|ll|ul|ull|f|d|ld)\s*\(/,
    message: "std::sto* は使わない。整数は std::from_chars (ec と全文字消費を検査)、浮動小数は共通ヘルパで変換する",
    why: "std::sto* は例外で失敗を通知し、'-1' を巨大な符号なし値に折り返す",
    suppressible: false,
  },
  {
    id: "C2.4", name: "gpu-struct-layout", severity: "warn",
    title: "GPU と共有する struct のレイアウト検査",
    files: PATHS.cpp, scope: "added-lines", check: checkGpuStructLayout,
    message: "vkCmdPushConstants / mapped memcpy に渡す型に static_assert(sizeof) と全フィールドの offsetof 検査がない",
    why: "C++ とシェーダのレイアウトがずれても、コンパイルも実行も黙って通り、壊れた値が GPU に渡る",
  },
  {
    id: "C2.5", name: "reinterpret-cast", severity: "warn",
    title: "reinterpret_cast の許可リスト",
    files: PATHS.cpp, scope: "added-lines", check: checkReinterpretCast,
    message: "reinterpret_cast は OS/C API の境界かバイト型との変換だけ。// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): <理由> (§C2.5) を付ける",
    why: "型パンは strict aliasing 違反の UB になりやすい。許可した箇所だけを注釈で数えられるようにする",
  },
  {
    id: "C2.6", name: "c-style-cast", severity: "warn",
    title: "C スタイルキャスト",
    files: PATHS.cpp, scope: "added-lines", check: checkCStyleCast,
    message: "C スタイルキャストは使わない。static_cast か、狭める変換は narrow<T>() を使う",
    why: "C スタイルキャストは const 外しや reinterpret を黙って選ぶ",
  },
  {
    id: "C2.6", name: "functional-cast", severity: "warn",
    title: "関数形式キャスト",
    files: PATHS.cpp, scope: "added-lines", check: checkFunctionalCast,
    message: "関数形式キャスト (int(x) 等) は使わない。static_cast か narrow<T>() を使う",
    why: "関数形式キャストは C スタイルキャストと同じ意味を持つ",
  },
  {
    id: "C2.6", name: "size-narrowing", severity: "warn",
    title: ".size() の縮小変換",
    files: PATHS.cpp, scope: "added-lines",
    pattern: /\bstatic_cast\s*<\s*(?:std::)?(?:int|short|long|unsigned(?:\s+(?:int|short))?|u?int(?:8|16|32)_t)\s*>\s*\(\s*[\w.:>\-[\]]*\.(?:size|length)\s*\(\s*\)\s*\)/,
    message: ".size() を狭い型へ static_cast しない。narrow<T>() で値が変わったら失敗を返す (直前の比較で上限を保証している場合を除く)",
    why: "要素数を uint16_t 等へ黙って切り詰めると、wire の長さと実データが食い違う",
  },
  {
    id: "C2.12", name: "shader-layout-sync", severity: "warn",
    title: "シェーダの型変更に C++ 側とレイアウトテストを伴わせる",
    files: PATHS.shader, scope: "changeset", check: checkShaderLayoutSync,
    message: "シェーダの struct/layout/buffer を変えたのに、対応する C++ ヘッダと shader_layout テストの変更が変更集合にない",
    why: "GPU 型の正本はシェーダ側にあり、C++ の写しとレイアウトテストを同じ PR で直さないとずれる",
  },
  {
    id: "C2.13", name: "utf8-path", severity: "warn",
    title: "パスと UTF-8 の変換は helper に集約",
    files: PATHS.cpp, scope: "added-lines",
    skip: (file, config) => inAny(config.ioHelpers, file),
    pattern: /\bu8path\s*\(|\b(?:std::filesystem|std::experimental::filesystem|fs)::path\s*\(\s*[A-Za-z_]\w*\s*\)|\b(?:MultiByteToWideChar|WideCharToMultiByte)\s*\(/,
    message: "std::string とパスの変換・UTF-8/UTF-16 変換は <repo>::io の helper (path_from_utf8 / path_to_utf8 / widen / narrow_utf8) を通す",
    why: "MSVC で std::string を path に渡すと ACP として解釈され、日本語パスが化ける",
  },

  // ---- C3 テスト ----
  {
    id: "C3.1", name: "test-framework-include", severity: "error",
    title: "テストフレームワークは Ergo の mini-gtest",
    files: PATHS.cpp, scope: "added-lines",
    pattern: /^[ \t]*#[ \t]*include[ \t]*[<"](?:catch2\/|catch\.hpp|doctest\/|doctest\.h|gmock\/)/m,
    message: "Catch2 / doctest / gmock は追加しない。Ergo の mini-gtest を使い、足りないマクロは Ergo へ PR で足す",
    why: "オフライン前提のリポで、テスト基盤が割れると consumer のビルドと CI 登録が揃わない",
  },
  {
    id: "C3.1", name: "test-framework-cmake", severity: "error",
    title: "外部のテストフレームワークを取得しない",
    files: PATHS.cmake, scope: "added-lines",
    pattern: /\b(?:FetchContent_Declare|ExternalProject_Add)\s*\(\s*(?:googletest|gtest|gmock|Catch2|catch2|doctest)\b|\bfind_package\s*\(\s*(?:GTest|GMock|Catch2|doctest)\b/i,
    message: "googletest / Catch2 / doctest を FetchContent・find_package で取り込まない。Ergo の mini-gtest と ergo_gtest_main を使う",
    why: "本家 gtest と mini-gtest が混ざると ODR 違反とリンクエラーになる",
  },
  {
    id: "C3.2", name: "tests-main", severity: "error",
    title: "tests/ に独自の main を作らない",
    files: PATHS.testSource, exclude: /(?:^|\/)tests?\/consumer\/|(?:^|\/)gtest_main\.(?:cpp|cc|cxx)$/, scope: "added-lines",
    pattern: /\b(?:int|auto)\s+w?main\s*\(/,
    message: "tests/ の下に独自の main を書かない。TEST/TEST_F で書き、共通の登録関数 (ergo_add_test 等) で main をリンクする",
    why: "独自 main のテストは CTest の登録・LABELS・baseline から漏れる",
  },
  {
    id: "C3.5", name: "concept-tests", severity: "warn",
    title: "concept は両側からテストする",
    files: PATHS.header, scope: "changeset", check: checkConceptTests,
    message: "concepts.h を変えたのに tests/<module>/test_<module>_concepts.cpp (満たす例と満たさない例の static_assert) の変更がない",
    why: "concept は受け入れすぎ (bool を算術型として通す等) が黙って起きるので、否定側を固定する",
  },
  {
    id: "C3.8", name: "fuzz-harness", severity: "warn",
    title: "デコーダに fuzz ハーネス",
    files: PATHS.cpp, scope: "changeset", check: checkFuzzHarness,
    message: "デコーダ (decoder/parser/codec/protocol/deserial/reader/save/scene・asset の loader) を変えたのに fuzz/<module>/fuzz_<target>.cpp がリポにも変更集合にもない",
    why: "外部入力を読むコードは、境界の取りこぼしを fuzz とコーパス回帰でしか固定できない",
  },
  {
    id: "C3.12", name: "augur-cpp", severity: "warn",
    title: "Augur に C/C++ を登録しない",
    files: PATHS.augurContracts, mask: "none", scope: "added-lines",
    pattern: /"[^"\n]*\.(?:c|cc|cpp|cxx|h|hh|hpp|hxx|inl|mm)"/,
    message: "augur.contracts.json は TS/JS 専用。C/C++ の契約は concept・static_assert・gtest の契約テストで表す",
    why: "Augur は TypeScript compiler API で動くので、C++ を登録しても何も検査されない",
  },

  // ---- C4 ビルド基盤 ----
  {
    id: "C4.1", name: "cxx-standard", severity: "warn",
    title: "C++ 標準は usage target だけが付与",
    files: PATHS.cmake, exclude: BASELINE_CMAKE, scope: "added-lines",
    pattern: /CXX_STANDARD\s+\d+|\bcxx_std_\d+\b|CXX_EXTENSIONS\s+(?:ON|TRUE|YES|1)\b/i,
    message: "CXX_STANDARD / cxx_std_* / CXX_EXTENSIONS ON を target や全体に書かない。標準は baseline の usage target が cxx_std_20 で付与する",
    why: "CXX_STANDARD プロパティは consumer に伝播せず、C++17 のホストが concepts.h を include すると壊れる",
  },
  {
    id: "C4.2", name: "per-target-utf8", severity: "warn",
    title: "/utf-8 は usage target に集約",
    files: PATHS.cmake, exclude: BASELINE_CMAKE, scope: "added-lines", check: checkPerTargetUtf8,
    message: "/utf-8 を target ごと・ディレクトリごとに付けない。baseline の usage target が PUBLIC で付与する",
    why: "個別付与は漏れ (非 ASCII を含むヘッダの target) と PUBLIC/PRIVATE の混在を生む",
  },
  {
    id: "C4.2", name: "source-nominmax", severity: "warn",
    title: "ソース内の NOMINMAX",
    files: PATHS.cpp, scope: "added-lines",
    pattern: /^[ \t]*#[ \t]*define[ \t]+(?:NOMINMAX|WIN32_LEAN_AND_MEAN)\b/m,
    message: "NOMINMAX / WIN32_LEAN_AND_MEAN をソースで #define しない。baseline の warnings target が付与する",
    why: "TU ごとの定義は windows.h の展開を TU ごとに変え、重複と漏れを生む",
  },
  {
    id: "C4.2", name: "public-math-defines", severity: "warn",
    title: "_USE_MATH_DEFINES を配布しない",
    files: PATHS.cmake, exclude: BASELINE_CMAKE, scope: "added-lines", check: checkPublicMathDefines,
    message: "_USE_MATH_DEFINES を PUBLIC / INTERFACE で配布しない。数学定数は std::numbers を使う",
    why: "consumer 全体のマクロ空間を汚す",
  },
  {
    id: "C4.5", name: "file-glob", severity: "warn",
    title: "file(GLOB) でソースを集めない",
    // tests/ 配下の *.cmake は `cmake -P` で実行する検査スクリプト (成果物を GLOB して中身を
    // 確かめる) で、ビルドのソースを集めないので見ない。tests/ の CMakeLists.txt は見る。
    files: PATHS.cmake, exclude: CMAKE_SCRIPT_OR_BASELINE, scope: "added-lines",
    pattern: /\bfile\s*\(\s*GLOB(?:_RECURSE)?\b/i,
    message: "file(GLOB) でソースを集めず、target_sources に列挙する",
    why: "GLOB だと Praesidium の登録対象を列挙できず、ファイル追加が再 configure まで反映されない",
  },
  {
    id: "C4.5", name: "directory-scope", severity: "warn",
    title: "ディレクトリスコープのコマンド",
    files: PATHS.cmake, exclude: BASELINE_CMAKE, scope: "added-lines",
    pattern: /^[ \t]*(?:add_compile_options|include_directories|link_libraries|link_directories|add_definitions|add_compile_definitions)\s*\(/im,
    message: "ディレクトリスコープの add_compile_options / include_directories / link_libraries 等は使わず、target_* で書く",
    why: "ディレクトリスコープの設定は consumer の target に漏れ、どの target に効くかが追えない",
  },
  {
    id: "C4.6", name: "cache-force", severity: "warn",
    title: "自分の option を CACHE FORCE で上書きしない",
    files: PATHS.cmake, scope: "added-lines", check: checkCacheForce,
    message: "自リポの option を CACHE ... FORCE で書き換えない。利用者が ON にした option の前提が欠けたら FATAL_ERROR にする",
    why: "利用者が ON にした機能が黙って OFF になる",
  },
  {
    id: "C4.7", name: "foreign-src", severity: "warn",
    title: "他リポの src/ を借りない",
    files: PATHS.cmake, exclude: BASELINE_CMAKE, scope: "added-lines", check: checkForeignSrc,
    message: "他リポの src/ (非公開ヘッダ・ソース) をパスで参照しない。提供側で include/<proj>/<mod>/ へ昇格させる PR を先に出す",
    why: "非公開ヘッダへの依存は提供側の内部変更で黙って壊れる",
  },
  {
    id: "C4.9", name: "os-in-public-header", severity: "warn",
    title: "公開ヘッダに OS 判定と OS SDK を書かない",
    files: PATHS.publicHeader, scope: "added-lines",
    pattern: new RegExp(String.raw`^[ \t]*#[ \t]*(?:if|ifdef|ifndef|elif)\b[^\n]*\b${OS_MACRO}\b|^[ \t]*#[ \t]*include[ \t]*<(?:windows\.h|Windows\.h|winsock2\.h|ws2tcpip\.h|unistd\.h|dlfcn\.h|pthread\.h|sys\/[\w/]+\.h|android\/[\w/]+\.h|TargetConditionals\.h)>`, "m"),
    message: "公開ヘッダで OS 判定マクロと OS SDK のヘッダを使わない。.cpp 側で分岐する",
    why: "公開ヘッダの OS 分岐は consumer のプラットフォームごとに API を変える",
  },
  {
    id: "C4.9", name: "platform-if-count", severity: "warn",
    title: "プラットフォーム分岐が 3 箇所以上なら分割",
    files: PATHS.source, exclude: PLATFORM_SPLIT_FILE, scope: "added-lines", check: checkPlatformIfCount,
    message: "1 つの .cpp にプラットフォーム条件の #if が 3 箇所以上ある。<name>_win.cpp / _posix.cpp に分割し CMake で選ぶ",
    why: "本体に #ifdef が混ざると、どの OS でどの経路が動くかが追えない",
  },
  {
    id: "C4.10", name: "header-default-macro", severity: "warn",
    title: "公開ヘッダで設定マクロの既定値を付けない",
    files: PATHS.publicHeader, scope: "added-lines",
    pattern: /^[ \t]*#[ \t]*ifndef[ \t]+(?<at>(?<name>[A-Za-z_]\w*))[ \t]*\r?\n[ \t]*#[ \t]*define[ \t]+\k<name>\b/m,
    filter: (m, ctx) => !/_(?:H|HH|HPP|HXX|INCLUDED)_?$/.test(m.groups.name) &&
      (!ctx.config.prefix || m.groups.name.startsWith(`${ctx.config.prefix}_`)),
    message: "公開ヘッダで #ifndef X / #define X の既定値を付けない。設定マクロは CMake option から PUBLIC target_compile_definitions で定義する",
    why: "TU ごとに -D が違うと同じ名前の inline 関数が 2 種類でき、リンカが黙って片方を選ぶ (ODR 違反)",
  },

  // ---- C5 失敗の型 ----
  {
    id: "C5.1", name: "string-err-out", severity: "warn",
    title: "std::string* err で失敗を返さない",
    files: PATHS.cpp, scope: "added-lines",
    pattern: /\bstd::string\s*\*\s*\w*[Ee]rr\w*/,
    message: "失敗を std::string* err の out 引数で返さない。[[nodiscard]] の Expected<T, <Module>Error> を返す",
    why: "out 引数のエラーは呼び出し側が無視でき、コンパイラも検出できない",
  },

  // ---- C6 所有権・寿命・並行・永続化 ----
  {
    id: "C6.1", name: "raw-pimpl", severity: "warn",
    title: "手書きの pimpl",
    files: PATHS.cpp, scope: "added-lines",
    pattern: /\bnew\s+Impl\b|\bdelete\s+impl_\b/,
    message: "pimpl は std::unique_ptr<Impl> で持ち、デストラクタを .cpp で定義する (new Impl / delete impl_ を書かない)",
    why: "手書きの new/delete は例外経路とムーブでリークか二重解放になる",
  },
  {
    id: "C6.4", name: "release-outside-helper", severity: "warn",
    title: ".release() は所有権の受け渡しヘルパに閉じる",
    files: PATHS.cpp, scope: "added-lines",
    skip: (file, config) => inAny(config.ownershipHelpers, file),
    pattern: /\.\s*release\s*\(\s*\)/,
    filter: releaseResultUsed,
    message: "unique_ptr::release() は C API へ所有権を渡すヘルパの中だけで呼ぶ (受け取り側の再取得と対にする)",
    why: "release と delete の経路が散ると二重解放とリークになる",
  },
  {
    id: "C6.5", name: "view-member", severity: "warn",
    title: "ビュー型をメンバに持てるのは *View だけ",
    files: PATHS.cpp, scope: "added-lines", check: checkViewMember,
    message: "string_view / span をメンバに持てるのは名前が *View で終わる型だけ。所有する型 (std::string / std::vector) で持つ",
    why: "ビューをメンバに持つと、参照先より長生きしてダングリングになる",
  },
  {
    id: "C6.6", name: "thread-safety-note", severity: "warn",
    title: "スレッド安全性の宣言",
    files: PATHS.header, scope: "file", check: checkThreadSafetyNote,
    message: "std::thread / mutex / atomic のメンバを持つヘッダに 'Thread-safety: thread-compatible|thread-safe|main-thread-only|<名前>-thread-only' の行がない",
    why: "どのスレッドから呼べるかが書かれていないと、ロックなしの書き換えがデータ競合になる",
  },
  {
    id: "C6.7", name: "singleton", severity: "error",
    title: "シングルトンを新しく作らない",
    files: PATHS.cpp, scope: "added-lines", check: checkSingleton,
    message: "static T& instance() を新しく作らない。依存はコンテキストかコンストラクタで注入する (例外は直前に // <PREFIX>_ALLOW_GLOBAL: <理由>)",
    why: "テスト間で状態が漏れ、DLL ごとに別インスタンスになり、初期化順序も不定になる",
    suppressible: false,
  },
  {
    id: "C6.8", name: "trunc-write", severity: "warn",
    title: "正本を trunc で直書きしない",
    files: PATHS.cpp, scope: "added-lines",
    skip: (file, config) => inAny(config.ioHelpers, file),
    pattern: /\b(?<at>ofstream)\b[^;]{0,300}?\btrunc\b/,
    message: "セーブ・設定・成果物を ofstream の trunc で直書きしない。atomic_write_file (tmp → fsync → rename) を使う",
    why: "書き込み途中のクラッシュで正本が空になる",
  },
  {
    id: "C6.8", name: "remove-before-rename", severity: "warn",
    title: "置き換え前に正本を消さない",
    files: PATHS.cpp, scope: "added-lines",
    skip: (file, config) => inAny(config.ioHelpers, file),
    check: checkRemoveBeforeRename,
    message: "rename の直前に remove_all で正本を消さない。置き換えは ReplaceFileW / rename の上書きで行う",
    why: "remove_all と rename の間にクラッシュすると成果物が消える",
  },

  // ---- C7 リアルタイム処理と決定性 (ロジック層だけ) ----
  {
    id: "C7.2", name: "logic-random", severity: "warn",
    title: "ロジック層で標準の分布と random_device を使わない",
    files: PATHS.cpp, scope: "added-lines",
    when: (config) => config.logicDirs.length > 0,
    skip: (file, config) => !inAny(config.logicDirs, file),
    pattern: /\b\w*_distribution\s*</ ,
    message: "ロジック層では std::*_distribution を使わず、共通の決定的 RNG (seed は外から注入) を使う",
    why: "分布クラスの実装は MSVC・libc++・libstdc++ で異なり、盤面がプラットフォームごとに変わる",
  },
  {
    id: "C7.2", name: "logic-random-device", severity: "warn",
    title: "ロジック層で random_device を使わない",
    files: PATHS.cpp, scope: "added-lines",
    when: (config) => config.logicDirs.length > 0,
    skip: (file, config) => !inAny(config.logicDirs, file),
    pattern: /\brandom_device\b/,
    message: "ロジック層では std::random_device を使わない。seed は外から注入する",
    why: "非決定な seed はリプレイとサーバ照合を壊す",
  },
  {
    id: "C7.3", name: "logic-clock", severity: "warn",
    title: "ロジック層で時計を直接読まない",
    files: PATHS.cpp, scope: "added-lines",
    when: (config) => config.logicDirs.length > 0,
    skip: (file, config) => !inAny(config.logicDirs, file),
    pattern: /\b\w+_clock\s*::\s*now\s*\(|(?<![\w.:]|->)time\s*\(\s*(?:nullptr|NULL|0)\s*\)/,
    message: "ロジック層では *_clock::now() を直接読まず、dt 引数か時計の interface で受け取る",
    why: "実時刻を読むとリプレイと固定 seed のテストが再現しない",
  },
  {
    id: "C7.4", name: "logic-libm", severity: "warn",
    title: "ロジック層で libm の超越関数を直接呼ばない",
    files: PATHS.cpp, scope: "added-lines",
    when: (config) => config.logicDirs.length > 0,
    skip: (file, config) => !inAny(config.logicDirs, file),
    pattern: /(?:\bstd::|(?<![\w.:]|->))(?:sin|cos|tan|asin|acos|atan|atan2|sinh|cosh|tanh|pow|exp|exp2|expm1|log|log2|log10|log1p|cbrt|hypot)f?\s*\(/,
    message: "ロジック層では std::sin/cos/tan/atan2/pow/exp/log 等を直接呼ばず、ergo_common の決定的な実装を通す",
    why: "libm の結果はプラットフォームごとに最下位ビットが違い、bit 一致のリプレイが崩れる",
  },

  // ---- C8 API の形・命名・依存の衛生 ----
  {
    id: "C8.2", name: "using-namespace-header", severity: "error",
    title: "ヘッダで using namespace を書かない",
    files: PATHS.header, scope: "added-lines",
    pattern: /\busing\s+namespace\b/,
    message: "ヘッダではどのスコープでも using namespace を書かない (詳細は ::detail に置く)",
    why: "ヘッダの using namespace は include したすべての TU の名前解決を変える",
  },
  {
    id: "C8.3", name: "positional-config", severity: "warn",
    title: "Config/Desc/Info は指示付き初期化子で構築",
    files: PATHS.cpp, scope: "added-lines", check: checkPositionalConfig,
    message: "*Config / *Desc / *Info を 2 要素以上の位置だけで初期化しない。{ .name = value } の指示付き初期化子で書く",
    why: "メンバを追加すると位置で初期化した値が黙ってずれる",
  },
  {
    id: "C8.4", name: "direct-output", severity: "warn",
    title: "ライブラリで標準出力へ直接書かない",
    files: PATHS.cpp, exclude: OUTPUT_EXEMPT_DIRS, scope: "added-lines",
    when: (config) => config.logger !== null,
    skip: (file, config) => inAny(config.logger.files, file),
    pattern: /\bstd::(?:cout|cerr|clog|wcout|wcerr)\b|(?:\bstd::|(?<![\w.:]|->))(?:printf|vprintf|puts)\s*\(|\b(?:std::)?(?:fprintf|vfprintf|fputs)\s*\(\s*(?:stderr|stdout)\b/,
    message: "ライブラリのコードで std::cout/cerr・printf・fprintf(stderr) を直接書かない。リポの共有ロガー (ERGO_LOG_* 等) を使う",
    why: "Android では stderr が消え、ログの経路が散るとレベルと出力先を制御できない",
  },
  {
    id: "C8.5", name: "sdk-in-public-header", severity: "warn",
    title: "公開ヘッダで SDK を直接 include しない",
    files: PATHS.publicHeader, exclude: /_fwd\.h$/, scope: "added-lines",
    pattern: /^[ \t]*#[ \t]*include[ \t]*[<"](?<sdk>vulkan|vk_mem_alloc|volk|fmod|curl|windows|winsock2|ws2tcpip|d3d1[12]|dxgi|jni)\b/im,
    filter: (m, ctx) => !ctx.config.publicIncludeAllow.includes(m.groups.sdk.toLowerCase()),
    message: "公開ヘッダで Vulkan / FMOD / curl / windows 等を直接 include しない。前方宣言ヘッダ (vk_fwd.h 等) か pimpl で隠す",
    why: "SDK のヘッダが consumer の全 TU に漏れ、マクロ汚染とビルド時間の増加を招く",
  },

  // ---- C9 機械担保と運用 ----
  {
    id: "C9.4", name: "nolint-form", severity: "error",
    title: "NOLINT の形式",
    files: PATHS.native, mask: "comments", scope: "added-lines", check: checkNolintForm,
    message: "抑制は // NOLINT(<check>): <理由> (§Cn) の形式 (NEXTLINE・BEGIN も同じ。END は check 名だけ) にする",
    why: "check 名と理由のない抑制は、何をなぜ黙らせたかが後から追えない",
    suppressible: false,
  },
  {
    id: "C9.4", name: "pragma-disable", severity: "error",
    title: "#pragma による警告の無効化",
    files: PATHS.native, scope: "added-lines", check: checkPragmaDisable,
    message: "#pragma warning(disable) / diagnostic ignored は直前の行に // <check>: <理由> (§Cn) を書き、push / pop で範囲を閉じる",
    why: "範囲を閉じない無効化はファイル末尾と include 先まで警告を消す",
    suppressible: false,
  },
];

// ---------------------------------------------------------------------------
// 規則表を束ねた公開 API
// ---------------------------------------------------------------------------

const RULE_KEYS = new Set(rules.flatMap((r) => [r.id, r.name]));
/**
 * disable に書けない規則 (§C9.5)。suppressible: false の規則名と、その規則を含む §C 番号
 * (§C 番号で無効にすると同じ番号の suppressible: false の規則まで止まるため)。
 */
export const LOCKED_RULE_KEYS = new Set(rules.filter((r) => r.suppressible === false).flatMap((r) => [r.id, r.name]));

/** 設定 (`cpp-rules.json` の内容) を検証して正規化する。disable のキーは規則表に実在し、無効にできる規則に限る。 */
export function parseRepoConfig(raw = {}) {
  return parseConfig(raw, { ruleKeys: RULE_KEYS, lockedKeys: LOCKED_RULE_KEYS });
}

/** `<repo>/cpp-rules.json` を読む。無ければ既定。壊れていれば例外 (黙って既定に戻さない)。 */
export function loadRepoConfig(repo) {
  return loadConfig(repo, { ruleKeys: RULE_KEYS, lockedKeys: LOCKED_RULE_KEYS });
}

/**
 * 1 ファイルに単一ファイルの規則 (scope が added-lines / file) を掛ける。hook はこれを使う。
 *
 * @param {string} file リポ相対パス (区切りは / か \)
 * @param {string} source ファイル全体の内容
 * @param {{ addedLines?: Iterable<number>|null, config?: object }} [options]
 *   addedLines: 追加行の行番号 (1 始まり)。null / 省略はファイル全体を追加行とみなす (新規ファイル・Write)。
 *   空なら (削除だけの変更) 何も報告しない。config: parseRepoConfig / loadRepoConfig の結果。
 * @returns {{id: string, rule: string, file: string, line: number, severity: "error"|"warn", message: string}[]}
 */
export function scanSource(file, source, { addedLines = null, config = parseRepoConfig({}) } = {}) {
  return runSource(rules, file, source, { addedLines, config });
}

/**
 * 変更集合に、単一ファイルの規則と変更集合の規則 (scope: changeset) を掛ける。gate はこれを使う。
 *
 * @param {{file: string, source: string, addedLines?: Iterable<number>|null}[]} changes
 * @param {{ config?: object, repoFiles?: string[] }} [options] repoFiles: リポの追跡ファイル一覧 (§C3.8 の既存ハーネス判定)
 */
export function scanChangeset(changes, { config = parseRepoConfig({}), repoFiles = [] } = {}) {
  return runChangeset(rules, changes, { config, repoFiles });
}

/** CLI (`node cpp-rules.mjs <repo> [--base <ref>] [--json]`)。戻り値は終了コード (0 / 1)。 */
export function main(argv, { stdout = process.stdout, stderr = process.stderr } = {}) {
  return runCli(argv, { scanChangeset, loadRepoConfig, stdout, stderr });
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exitCode = main(process.argv.slice(2));
}
