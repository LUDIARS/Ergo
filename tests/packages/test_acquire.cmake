# Acquisition-stage rules (ErgoPackageRulesAcquire.cmake) and the extra
# manifest merge: PKG-03/04/05/07/08/09/10/16 true and false, the drift
# classification, required targets per feature, STRICT escalation.
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")

set(PIN 625d83d61c90c30ceed3fab2bd2e11a7edf8d39f)
set(OTHER 4142f77c0775e1c3ad93733526085c56523ea9dc)

# --- drift classification ------------------------------------------------------
ergo_pkg_drift_classify(d HEAD "" PIN ${PIN} PIN_KNOWN FALSE PIN_IS_ANCESTOR FALSE HEAD_IS_ANCESTOR FALSE)
pkg_assert_eq("no HEAD" "${d}" unknown)
ergo_pkg_drift_classify(d HEAD ${PIN} PIN ${PIN} PIN_KNOWN TRUE PIN_IS_ANCESTOR TRUE HEAD_IS_ANCESTOR TRUE)
pkg_assert_eq("same" "${d}" none)
ergo_pkg_drift_classify(d HEAD ${OTHER} PIN ${PIN} PIN_KNOWN FALSE PIN_IS_ANCESTOR FALSE HEAD_IS_ANCESTOR FALSE)
pkg_assert_eq("pin missing" "${d}" unknown)
ergo_pkg_drift_classify(d HEAD ${OTHER} PIN ${PIN} PIN_KNOWN TRUE PIN_IS_ANCESTOR TRUE HEAD_IS_ANCESTOR FALSE)
pkg_assert_eq("ahead" "${d}" ahead)
ergo_pkg_drift_classify(d HEAD ${OTHER} PIN ${PIN} PIN_KNOWN TRUE PIN_IS_ANCESTOR FALSE HEAD_IS_ANCESTOR TRUE)
pkg_assert_eq("behind" "${d}" behind)
ergo_pkg_drift_classify(d HEAD ${OTHER} PIN ${PIN} PIN_KNOWN TRUE PIN_IS_ANCESTOR FALSE HEAD_IS_ANCESTOR FALSE)
pkg_assert_eq("diverged" "${d}" diverged)
pkg_pass("drift classification")

# --- PKG-05 / PKG-09 -----------------------------------------------------------
ergo_pkg_rule_drift(f PACKAGE p ROUTE source DRIFT none HEAD ${PIN} PIN ${PIN})
pkg_assert_empty("PKG-05 none" "${f}")
foreach(drift ahead behind diverged)
    ergo_pkg_rule_drift(f PACKAGE p ROUTE host DRIFT ${drift} HEAD ${OTHER} PIN ${PIN})
    pkg_assert_finding("PKG-05 ${drift}" "${f}" PKG-05 warning p)
    ergo_pkg_rule_drift(f PACKAGE p ROUTE host DRIFT ${drift} HEAD ${OTHER} PIN ${PIN} STRICT ON)
    pkg_assert_finding("PKG-05 ${drift} strict" "${f}" PKG-05 error p)
endforeach()
ergo_pkg_rule_drift(f PACKAGE p ROUTE sibling DRIFT unknown HEAD ${OTHER} PIN ${PIN})
pkg_assert_finding("PKG-05 pin missing" "${f}" PKG-05 warning p)
ergo_pkg_rule_drift(f PACKAGE p ROUTE source DRIFT unknown HEAD "" PIN ${PIN})
pkg_assert_finding("PKG-09 no git" "${f}" PKG-09 warning p)
ergo_pkg_rule_drift(f PACKAGE p ROUTE source DRIFT unknown HEAD "" PIN ${PIN} STRICT ON)
pkg_assert_finding("PKG-09 strict" "${f}" PKG-09 error p)
ergo_pkg_rule_drift(f PACKAGE p ROUTE installed DRIFT unknown HEAD "" PIN ${PIN} VERSION 1.0)
pkg_assert_finding("installed is recorded" "${f}" PKG-09 record p)
ergo_pkg_rule_drift(f PACKAGE p ROUTE fetch DRIFT diverged HEAD ${OTHER} PIN ${PIN})
pkg_assert_empty("fetch left to PKG-04" "${f}")
pkg_pass("PKG-05 / PKG-09")

# --- PKG-04 --------------------------------------------------------------------
ergo_pkg_rule_fetched_head(f PACKAGE p PIN ${PIN} HEAD ${PIN})
pkg_assert_empty("PKG-04 equal" "${f}")
ergo_pkg_rule_fetched_head(f PACKAGE p PIN ${PIN} HEAD ${OTHER})
pkg_assert_finding("PKG-04 differ" "${f}" PKG-04 error p)
ergo_pkg_rule_fetched_head(f PACKAGE p PIN ${PIN} HEAD "")
pkg_assert_finding("PKG-04 unreadable" "${f}" PKG-04 error p)
pkg_pass("PKG-04")

# --- PKG-03 --------------------------------------------------------------------
ergo_pkg_rule_unresolved(f PACKAGE my-pkg TRIED host sibling "fetch(disabled)")
pkg_assert_finding("PKG-03" "${f}" PKG-03 error my-pkg)
if(NOT f MATCHES "ERGO_PACKAGE_SOURCE_DIR_MY_PKG" OR NOT f MATCHES "tried: host, sibling, fetch\\(disabled\\)")
    pkg_fail("PKG-03 message lacks the routes / variable: ${f}")
endif()
pkg_pass("PKG-03")

# --- PKG-07 / PKG-08 / PKG-16 ------------------------------------------------------
ergo_pkg_rule_dirty(f PACKAGE p DIRTY FALSE)
pkg_assert_empty("PKG-07 clean" "${f}")
ergo_pkg_rule_dirty(f PACKAGE p DIRTY TRUE)
pkg_assert_finding("PKG-07 record" "${f}" PKG-07 record p)
ergo_pkg_rule_dirty(f PACKAGE p DIRTY TRUE STRICT ON)
pkg_assert_finding("PKG-07 strict" "${f}" PKG-07 error p)

ergo_pkg_rule_expect_tag(f PACKAGE p TAG v1.0.0 TAG_FOUND TRUE PIN_DESCENDS TRUE PIN ${PIN})
pkg_assert_empty("PKG-08 descends" "${f}")
ergo_pkg_rule_expect_tag(f PACKAGE p TAG v1.0.0 TAG_FOUND FALSE PIN_DESCENDS FALSE PIN ${PIN})
pkg_assert_empty("PKG-08 tag absent" "${f}")
ergo_pkg_rule_expect_tag(f PACKAGE p TAG "" TAG_FOUND TRUE PIN_DESCENDS FALSE PIN ${PIN})
pkg_assert_empty("PKG-08 no tag" "${f}")
ergo_pkg_rule_expect_tag(f PACKAGE p TAG v1.0.0 TAG_FOUND TRUE PIN_DESCENDS FALSE PIN ${PIN})
pkg_assert_finding("PKG-08" "${f}" PKG-08 warning p)
ergo_pkg_rule_expect_tag(f PACKAGE p TAG v1.0.0 TAG_FOUND TRUE PIN_DESCENDS FALSE PIN ${PIN} STRICT ON)
pkg_assert_finding("PKG-08 strict" "${f}" PKG-08 error p)

ergo_pkg_rule_submodule(f PACKAGE t PATH lib/t INITIALIZED TRUE)
pkg_assert_empty("PKG-16 initialised" "${f}")
ergo_pkg_rule_submodule(f PACKAGE t PATH lib/t INITIALIZED FALSE)
pkg_assert_finding("PKG-16" "${f}" PKG-16 warning t)
ergo_pkg_rule_submodule(f PACKAGE t PATH lib/t INITIALIZED FALSE STRICT ON)
pkg_assert_finding("PKG-16 strict" "${f}" PKG-16 error t)
pkg_pass("PKG-07 / PKG-08 / PKG-16")

# --- required targets + PKG-10 -------------------------------------------------------
# Pictor-shaped: pictor is claimed by vulkan, pictor_webgl by web.
ergo_pkg_required_targets(r TARGETS pictor SELECTED vulkan
    PROVIDES "vulkan=pictor" "web=pictor_webgl" "webgpu=")
pkg_assert_eq("desktop requires" "${r}" pictor)
ergo_pkg_required_targets(r TARGETS pictor SELECTED web
    PROVIDES "vulkan=pictor" "web=pictor_webgl")
pkg_assert_eq("emscripten requires" "${r}" pictor_webgl)
ergo_pkg_required_targets(r TARGETS "Tela::Core;Tela::Graph" SELECTED "" PROVIDES "")
pkg_assert_eq("unclaimed targets always" "${r}" "Tela::Core;Tela::Graph")

ergo_pkg_rule_layout(f PACKAGE pictor ROUTE source REQUIRED pictor PRESENT pictor
    CONTRACTS "vulkan:pictor:PICTOR_HAS_VULKAN" HONOURED "vulkan:pictor:PICTOR_HAS_VULKAN")
pkg_assert_empty("PKG-10 complete" "${f}")
ergo_pkg_rule_layout(f PACKAGE pictor ROUTE fetch REQUIRED pictor PRESENT "")
pkg_assert_finding("PKG-10 target missing" "${f}" PKG-10 error pictor)
ergo_pkg_rule_layout(f PACKAGE pictor ROUTE host REQUIRED pictor PRESENT pictor
    CONTRACTS "vulkan:pictor:PICTOR_HAS_VULKAN" HONOURED "")
pkg_assert_finding("PKG-10 contract missing" "${f}" PKG-10 error pictor/vulkan)
ergo_pkg_rule_layout(f PACKAGE tela ROUTE source REQUIRED "Tela::Core" PRESENT "Tela::Core"
    CONTRACTS "core:Tela::Core:TELA_X" HONOURED "core:Tela::Core:TELA_X")
pkg_assert_empty("PKG-10 namespaced target" "${f}")
pkg_pass("required targets / PKG-10")

# --- extra manifest merge (PKG-15 name clash) ---------------------------------------
file(MAKE_DIRECTORY "${ERGO_TEST_WORK_DIR}")
file(WRITE "${ERGO_TEST_WORK_DIR}/extra.json" [=[
{ "schema": 1,
  "packages": {
    "private-lib": { "kind": "link", "code": "Px", "repository": "https://example.invalid/PrivateLib.git",
                     "rev": "625d83d61c90c30ceed3fab2bd2e11a7edf8d39f",
                     "cmake": { "wiring": "private.cmake", "targets": ["private_lib"] } },
    "tela": { "kind": "link", "rev": "625d83d61c90c30ceed3fab2bd2e11a7edf8d39f" }
  } }
]=])
ergo_pkg_manifest_load(m "${ERGO_SOURCE_DIR}/ergo-packages.json")
ergo_pkg_manifest_load(x "${ERGO_TEST_WORK_DIR}/extra.json")
ergo_pkg_manifest_merge(m x f)
pkg_assert_finding("clash" "${f}" PKG-15 error tela)
pkg_assert_eq("merged link" "${m_LINK}" "pictor;private-lib;tela")
pkg_assert_eq("merged packages" "${m_PACKAGES}" "lapilli;pictor;private-lib;tela")
pkg_assert_eq("merged targets" "${m_PKG_private-lib_TARGETS}" private_lib)
pkg_assert_eq("public tela kept" "${m_PKG_tela_CODE}" Tl)
pkg_assert_eq("repo name" "${m_PKG_private-lib_REPO_NAME}" PrivateLib)
pkg_assert_eq("wiring relative to the extra manifest" "${m_PKG_private-lib_WIRING}"
    "${ERGO_TEST_WORK_DIR}/private.cmake")
pkg_assert_eq("public wiring" "${m_PKG_pictor_WIRING}" "${ERGO_SOURCE_DIR}/cmake/packages/pictor.cmake")
pkg_assert_eq("pictor exports" "${m_PKG_pictor_EXPORTS}" "PICTOR_PROFILE_DIR=profiles;PICTOR_SHADER_DIR=shaders")
pkg_assert_eq("web feature cache" "${m_PKG_pictor_FEAT_web_CACHE}" "PICTOR_BUILD_WEBGL=ON")
pkg_pass("extra manifest merge")
