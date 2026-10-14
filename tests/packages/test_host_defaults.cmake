# Host declaration defaults (spec/feature/package-manager.md §2.1):
#   * ergo_pkg_manifest_load(... HOST) reads host.use / optional / features /
#     legacy_vars and Ergo's pin
#   * ergo_pkg_host_default: a non-empty cache value wins ("cache"), an
#     empty one takes the declaration ("declaration"), no declaration and no
#     cache value is "none"
#   * ergo_pkg_host_feature_default / ergo_pkg_host_legacy_source
#   * ergo_pkg_rule_optional_unused records (never an error)
#   * resolved.json: the host section carries the defaults' origin and Ergo's
#     pin only when a host declaration was read
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir> -P tests/packages/test_host_defaults.cmake
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")
include("${ERGO_SOURCE_DIR}/cmake/packages/ErgoPackageHostDefaults.cmake")

file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)
file(REMOVE_RECURSE "${W}")
file(MAKE_DIRECTORY "${W}")
set(SHA 1111111111111111111111111111111111111111)

# --- the loader ---------------------------------------------------------------
file(WRITE "${W}/decl.json" "{
  \"schema\": 1,
  \"host\": {
    \"code\": \"Example\",
    \"ergo\": { \"repository\": \"https://github.com/LUDIARS/Ergo.git\", \"rev\": \"${SHA}\", \"expect_tag\": \"v0.9.0\" },
    \"use\": [\"pictor\", \"tela\"],
    \"optional\": [\"pictor\"],
    \"features\": { \"tela\": \"web\", \"pictor\": \"+vulkan,-web\", \"other\": \"vulkan;web\" },
    \"legacy_vars\": { \"EXAMPLE_ERGO_DIR\": \"ergo.source_dir\", \"EXAMPLE_PICTOR_DIR\": \"pictor.source_dir\" }
  },
  \"packages\": {}
}
")
ergo_pkg_manifest_load(d "${W}/decl.json" HOST)
pkg_assert_eq("host.use" "${d_HOST_USE}" "pictor;tela")
pkg_assert_eq("host.optional" "${d_HOST_OPTIONAL}" "pictor")
pkg_assert_eq("host.features (sorted, comma-joined)" "${d_HOST_FEATURES}"
    "other=vulkan,web;pictor=+vulkan,-web;tela=web")
pkg_assert_eq("host.ergo.rev" "${d_HOST_ERGO_REV}" "${SHA}")
pkg_assert_eq("host.ergo.expect_tag" "${d_HOST_ERGO_EXPECT_TAG}" "v0.9.0")
pkg_assert_eq("host.legacy_vars" "${d_HOST_LEGACY_VARS}"
    "EXAMPLE_ERGO_DIR=ergo.source_dir;EXAMPLE_PICTOR_DIR=pictor.source_dir")

file(WRITE "${W}/legacy-extra.json" "{ \"schema\": 1, \"packages\": {} }\n")
ergo_pkg_manifest_load(x "${W}/legacy-extra.json" HOST)
pkg_assert_eq("legacy extra: no host" "${x_HOST_PRESENT}" "FALSE")
pkg_assert_eq("legacy extra: no use" "${x_HOST_USE}" "")
pkg_pass("loader reads the host defaults")

# --- ergo_pkg_host_default -----------------------------------------------------
ergo_pkg_host_default(v from DECLARED TRUE CACHE DEFAULT pictor tela)
pkg_assert_eq("empty cache takes the declaration" "${v}" "pictor;tela")
pkg_assert_eq("from declaration" "${from}" "declaration")
ergo_pkg_host_default(v from DECLARED TRUE CACHE tela DEFAULT pictor tela)
pkg_assert_eq("cache wins" "${v}" "tela")
pkg_assert_eq("from cache" "${from}" "cache")
ergo_pkg_host_default(v from DECLARED TRUE CACHE DEFAULT)
pkg_assert_eq("declared empty" "${v}" "")
pkg_assert_eq("declared empty is still the declaration" "${from}" "declaration")
ergo_pkg_host_default(v from DECLARED FALSE CACHE DEFAULT pictor)
pkg_assert_eq("no declaration, no cache" "${v}" "")
pkg_assert_eq("from none" "${from}" "none")
ergo_pkg_host_default(v from DECLARED FALSE CACHE pictor DEFAULT)
pkg_assert_eq("no declaration: the cache" "${from}" "cache")
pkg_pass("cache value over declaration default")

# --- features / legacy aliases -------------------------------------------------
ergo_pkg_host_feature_default(t PACKAGE pictor PAIRS ${d_HOST_FEATURES})
pkg_assert_eq("feature tokens" "${t}" "+vulkan;-web")
ergo_pkg_host_feature_default(t PACKAGE other PAIRS ${d_HOST_FEATURES})
pkg_assert_eq("semicolon form" "${t}" "vulkan;web")
ergo_pkg_host_feature_default(t PACKAGE gamma PAIRS ${d_HOST_FEATURES})
pkg_assert_eq("no features declared" "${t}" "")
ergo_pkg_host_legacy_source(l PACKAGE pictor PAIRS ${d_HOST_LEGACY_VARS})
pkg_assert_eq("legacy alias of pictor" "${l}" "EXAMPLE_PICTOR_DIR")
ergo_pkg_host_legacy_source(l PACKAGE tela PAIRS ${d_HOST_LEGACY_VARS})
pkg_assert_eq("no alias of tela" "${l}" "")
pkg_pass("feature request and legacy alias of the declaration")

# --- optional packages are recorded, never an error ----------------------------
ergo_pkg_rule_optional_unused(f PACKAGE pictor TRIED host sibling "fetch(disabled)")
pkg_assert_finding("optional unused" "${f}" PKG-03 record pictor)
ergo_pkg_findings_with_severity(e error ${f})
pkg_assert_empty("optional unused is no error" "${e}")
pkg_pass("optional package not found is a record")

# --- resolved.json host section ------------------------------------------------
set(r_MODE M1)
set(r_HOST_ID Example)
ergo_pkg_report_render(plain r)
string(JSON keys LENGTH "${plain}" host)
pkg_assert_eq("no declaration: host has only id" "${keys}" "1")

set(r_HOST_DECLARED TRUE)
set(r_HOST_SELECTED_FROM declaration)
set(r_HOST_OPTIONAL pictor)
set(r_HOST_OPTIONAL_FROM declaration)
set(r_HOST_UNUSED "")
set(r_HOST_FEATURES_FROM "pictor=cache" "tela=declaration")
set(r_HOST_ERGO_PIN "${SHA}")
set(r_HOST_ERGO_ROUTE source)
set(r_HOST_ERGO_HEAD "${SHA}")
set(r_HOST_ERGO_DRIFT none)
set(r_HOST_ERGO_DIRTY FALSE)
ergo_pkg_report_render(json r)
ergo_pkg_report_render(json2 r)
pkg_assert_eq("deterministic" "${json}" "${json2}")
string(JSON v GET "${json}" host selected_from)
pkg_assert_eq("selected_from" "${v}" "declaration")
string(JSON v GET "${json}" host optional 0)
pkg_assert_eq("optional" "${v}" "pictor")
string(JSON v GET "${json}" host features_from pictor)
pkg_assert_eq("features_from cache" "${v}" "cache")
string(JSON v GET "${json}" host features_from tela)
pkg_assert_eq("features_from declaration" "${v}" "declaration")
string(JSON v GET "${json}" host ergo route)
pkg_assert_eq("ergo route" "${v}" "source")
string(JSON v GET "${json}" host ergo drift)
pkg_assert_eq("ergo drift" "${v}" "none")
string(JSON v GET "${json}" host ergo dirty)
pkg_assert_eq("ergo dirty" "${v}" "OFF")

set(r_HOST_ERGO_PIN "")
ergo_pkg_report_render(json r)
string(JSON t TYPE "${json}" host ergo)
pkg_assert_eq("no Ergo pin: ergo is null" "${t}" "NULL")
pkg_pass("resolved.json host section")
