/** RULE_CODE_CPP §C9.5 規則表: パスの分類と言語判定。パスはリポ相対・区切りは `/`。 */

/** 規則の `files` に使うパス分類。パスはリポ相対・区切りは `/`。 */
export const PATHS = Object.freeze({
  /** C と C++ のソースとヘッダ (C にも当てはまる規則: 禁止 API・実行時ロード・抑制の形式)。 */
  native: /\.(?:c|cc|cpp|cxx|h|hh|hpp|hxx|inl|ipp|tpp|mm|ixx|cppm|mpp)$/i,
  /** C++ のソースとヘッダ (Objective-C++ とモジュール単位を含む。.c は含まない)。 */
  cpp: /\.(?:cc|cpp|cxx|h|hh|hpp|hxx|inl|ipp|tpp|mm|ixx|cppm|mpp)$/i,
  /** 翻訳単位になるソース。 */
  source: /\.(?:c|cc|cpp|cxx|mm)$/i,
  /** ヘッダ。 */
  header: /\.(?:h|hh|hpp|hxx|inl|ipp|tpp)$/i,
  /** 公開ヘッダ (`include/` 配下)。 */
  publicHeader: /(?:^|\/)include\/.+\.(?:h|hh|hpp|hxx|inl|ipp|tpp)$/i,
  /** C++20 モジュールのインターフェース単位。 */
  moduleUnit: /\.(?:ixx|cppm|mpp)$/i,
  /** CMake。 */
  cmake: /(?:^|\/)CMakeLists\.txt$|\.cmake$/i,
  /** シェーダ。 */
  shader: /\.(?:glsl|comp|vert|frag|geom|tesc|tese|mesh|task|rgen|rchit|rmiss|rahit|rint|rcall)$/i,
  /** tests/ 配下の翻訳単位。 */
  testSource: /(?:^|\/)tests?\/.*\.(?:c|cc|cpp|cxx|mm)$/i,
  /** Augur の契約登録ファイル。 */
  augurContracts: /(?:^|\/)augur\.contracts\.json$/,
});

/** どの規則も見ないパス。third_party は §C4.8 の同梱条件だけ (別の検査) を適用する。 */
export const GLOBAL_EXCLUDE =
  /(?:^|\/)(?:third_party|3rdparty|external|extern|vendor|node_modules|\.git)\/|^(?:[^/]+\/)?(?:build|out|cmake-build-[^/]+)\//i;
export const BASELINE_CMAKE = /(?:^|\/)LudiarsCxxBaseline\.cmake$/;
/** §C4.5 file-glob の除外: baseline と、tests/ 配下の `cmake -P` 用スクリプト (*.cmake)。 */
export const CMAKE_SCRIPT_OR_BASELINE = /(?:^|\/)LudiarsCxxBaseline\.cmake$|(?:^|\/)tests?\/(?:.*\/)?[^/]+\.cmake$/i;
export const TESTS_DIR = /(?:^|\/)tests?\//;
/** §C8.4: 直接出力を許す場所 (CLI・demo・tools・tests の main)。 */
export const OUTPUT_EXEMPT_DIRS = /(?:^|\/)(?:tests?|demos?|examples?|tools|cli|fuzz|bench|benchmarks?)\//;
/** §C4.9: OS ごとに分割済みのファイル。 */
export const PLATFORM_SPLIT_FILE = /_(?:win|win32|windows|posix|unix|linux|android|apple|mac|macos|ios|darwin)\.[^/.]+$/i;
export const OS_MACRO = String.raw`(?:_WIN32|_WIN64|__APPLE__|__ANDROID__|__linux__|__linux|__unix__|__unix|__MACH__|__EMSCRIPTEN__|WINAPI_FAMILY|TARGET_OS_\w+)`;

/** ファイルの言語。規則表の対象外なら null。 */
export function languageOf(file) {
  if (PATHS.cmake.test(file)) return "cmake";
  if (PATHS.native.test(file)) return "cpp";
  if (PATHS.shader.test(file)) return "shader";
  if (PATHS.augurContracts.test(file)) return "json";
  return null;
}

export function normalizePath(file) {
  return String(file).replace(/\\/g, "/").replace(/^\.\//, "");
}
