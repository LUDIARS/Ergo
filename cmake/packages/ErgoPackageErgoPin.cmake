# ---------------------------------------------------------------------------
# ErgoPackageErgoPin.cmake — the acquisition checks applied to Ergo itself
#
# A host declaration pins the Ergo it builds on (host.ergo.rev,
# spec/feature/package-manager.md §2.1). The bootstrap only checks the pin's
# form (PKG-02) before a fetch; this file runs the same checks a link
# package gets once Ergo is configured (spec/feature/package-hub.md §4):
#   PKG-02  host.ergo.rev is not a 40-digit SHA
#   PKG-04  route fetch: the fetched HEAD is not the pin
#   PKG-05  other routes: the checkout drifts from the pin (PKG-09 when the
#           HEAD cannot be read)
#   PKG-07  the checkout has uncommitted changes
#   PKG-08  expect_tag present and the pin does not descend from it
# The route is the bootstrap's ERGO_BOOTSTRAP_ROUTE (source / sibling /
# fetch), or "host" when the host added Ergo itself. Subject is "ergo".
#
#   ergo_pkg_check_ergo_pin(<prefix> ROOT <dir> PIN <sha> ROUTE <route>
#                           [EXPECT_TAG <tag>] [STRICT <bool>])
# Sets <prefix>_HEAD / _DRIFT / _DIRTY / _FINDINGS in the caller's scope.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageRules.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageGit.cmake")

function(ergo_pkg_check_ergo_pin prefix)
    cmake_parse_arguments(A "" "ROOT;PIN;ROUTE;EXPECT_TAG;STRICT" "" ${ARGN})
    set(_findings "")
    ergo_pkg_rule_rev_format(_f PACKAGE ergo REV "${A_PIN}" FIELD "host.ergo.rev")
    list(APPEND _findings ${_f})

    # ROOT_ONLY: a vendored copy inside the host repository must not report
    # the host's HEAD as Ergo's.
    ergo_pkg_git_probe(_g "${A_ROOT}" "${A_PIN}" "${A_EXPECT_TAG}" ROOT_ONLY)
    ergo_pkg_drift_classify(_drift HEAD "${_g_HEAD}" PIN "${A_PIN}"
        PIN_KNOWN ${_g_PIN_KNOWN} PIN_IS_ANCESTOR ${_g_PIN_IS_ANCESTOR}
        HEAD_IS_ANCESTOR ${_g_HEAD_IS_ANCESTOR})
    if(A_ROUTE STREQUAL "fetch")
        ergo_pkg_rule_fetched_head(_f PACKAGE ergo PIN "${A_PIN}" HEAD "${_g_HEAD}")
        list(APPEND _findings ${_f})
    endif()
    ergo_pkg_rule_drift(_f PACKAGE ergo ROUTE "${A_ROUTE}" DRIFT "${_drift}"
        HEAD "${_g_HEAD}" PIN "${A_PIN}" STRICT "${A_STRICT}")
    list(APPEND _findings ${_f})
    ergo_pkg_rule_dirty(_f PACKAGE ergo DIRTY ${_g_DIRTY} STRICT "${A_STRICT}")
    list(APPEND _findings ${_f})
    ergo_pkg_rule_expect_tag(_f PACKAGE ergo TAG "${A_EXPECT_TAG}"
        TAG_FOUND ${_g_TAG_FOUND} PIN_DESCENDS ${_g_PIN_DESCENDS} PIN "${A_PIN}"
        STRICT "${A_STRICT}")
    list(APPEND _findings ${_f})

    set(${prefix}_HEAD "${_g_HEAD}" PARENT_SCOPE)
    set(${prefix}_DRIFT "${_drift}" PARENT_SCOPE)
    set(${prefix}_DIRTY "${_g_DIRTY}" PARENT_SCOPE)
    set(${prefix}_FINDINGS "${_findings}" PARENT_SCOPE)
endfunction()
