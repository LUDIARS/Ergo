# Rule tests: PKG-01 / 02 / 06 / 11 / 14 / 15, each in its true and false
# direction (PKG-12 in test_cycles.cmake, PKG-13 in test_features.cmake).
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")

set(SHA_A "625d83d61c90c30ceed3fab2bd2e11a7edf8d39f")
set(SHA_B "4142f77c0775e1c3ad93733526085c56523ea9dc")

# --- PKG-01 unknown names --------------------------------------------------
ergo_pkg_rule_unknown_names(f SELECTED pictor tela KNOWN pictor tela)
pkg_assert_empty("PKG-01 known names" "${f}")
ergo_pkg_rule_unknown_names(f SELECTED pictor picter KNOWN pictor tela)
pkg_assert_finding("PKG-01 unknown name" "${f}" PKG-01 error picter)
ergo_pkg_rule_unknown_names(f SELECTED lapilli KNOWN pictor tela)
pkg_assert_finding("PKG-01 tool is not selectable" "${f}" PKG-01 error lapilli)
ergo_pkg_rule_unknown_names(f SELECTED KNOWN pictor tela)
pkg_assert_empty("PKG-01 empty selection" "${f}")
pkg_pass("PKG-01")

# --- PKG-02 rev format -----------------------------------------------------
ergo_pkg_rule_rev_format(f PACKAGE pictor REV "${SHA_A}")
pkg_assert_empty("PKG-02 full sha" "${f}")
foreach(bad "main" "625d83d" "v2.2.0" "" "625D83D61C90C30CEED3FAB2BD2E11A7EDF8D39F"
        "${SHA_A}0" "g25d83d61c90c30ceed3fab2bd2e11a7edf8d39f")
    ergo_pkg_rule_rev_format(f PACKAGE pictor REV "${bad}")
    pkg_assert_finding("PKG-02 rejects '${bad}'" "${f}" PKG-02 error pictor)
endforeach()
pkg_pass("PKG-02")

# --- PKG-06 rev override ---------------------------------------------------
ergo_pkg_rule_rev_override(f PACKAGE pictor PIN "${SHA_A}" OVERRIDE "" STRICT OFF)
pkg_assert_empty("PKG-06 no override" "${f}")
ergo_pkg_rule_rev_override(f PACKAGE pictor PIN "${SHA_A}" OVERRIDE "${SHA_A}" STRICT ON)
pkg_assert_empty("PKG-06 override equal to pin" "${f}")
ergo_pkg_rule_rev_override(f PACKAGE pictor PIN "${SHA_A}" OVERRIDE "${SHA_B}" STRICT OFF)
pkg_assert_finding("PKG-06 override warns" "${f}" PKG-06 warning pictor)
pkg_assert_no_finding("PKG-06 valid override sha" "${f}" PKG-02)
ergo_pkg_rule_rev_override(f PACKAGE pictor PIN "${SHA_A}" OVERRIDE "${SHA_B}" STRICT ON)
pkg_assert_finding("PKG-06 override strict" "${f}" PKG-06 error pictor)
ergo_pkg_rule_rev_override(f PACKAGE pictor PIN "${SHA_A}" OVERRIDE "main" STRICT OFF)
pkg_assert_finding("PKG-06 override must be a sha" "${f}" PKG-02 error pictor)
pkg_pass("PKG-06")

# --- PKG-11 requires / cmake minimum ---------------------------------------
ergo_pkg_rule_requires(f PACKAGE tela REQUIRES pictor AVAILABLE pictor tela)
pkg_assert_empty("PKG-11 requires met" "${f}")
ergo_pkg_rule_requires(f PACKAGE tela REQUIRES AVAILABLE tela)
pkg_assert_empty("PKG-11 no requires" "${f}")
ergo_pkg_rule_requires(f PACKAGE tela REQUIRES pictor AVAILABLE tela)
pkg_assert_finding("PKG-11 requires unmet" "${f}" PKG-11 error tela)
ergo_pkg_rule_cmake_minimum(f PACKAGE pictor MINIMUM 3.20 CURRENT 3.20.0)
pkg_assert_empty("PKG-11 minimum equal" "${f}")
ergo_pkg_rule_cmake_minimum(f PACKAGE pictor MINIMUM 3.20 CURRENT 4.1.2)
pkg_assert_empty("PKG-11 minimum older" "${f}")
ergo_pkg_rule_cmake_minimum(f PACKAGE pictor MINIMUM "" CURRENT 3.20)
pkg_assert_empty("PKG-11 minimum absent" "${f}")
ergo_pkg_rule_cmake_minimum(f PACKAGE pictor MINIMUM 3.28 CURRENT 3.20.6)
pkg_assert_finding("PKG-11 minimum newer" "${f}" PKG-11 error pictor)
pkg_pass("PKG-11")

# --- PKG-14 module needs ---------------------------------------------------
ergo_pkg_rule_module_needs(f MODULE ergo_render ENABLED TRUE NEEDS pictor
    AVAILABLE pictor NEED_KIND real-path REAL_PATH TRUE MODE selected)
pkg_assert_empty("PKG-14 need met (real path)" "${f}")
ergo_pkg_rule_module_needs(f MODULE ergo_render ENABLED FALSE NEEDS pictor
    AVAILABLE "" MODE selected)
pkg_assert_empty("PKG-14 module OFF" "${f}")
ergo_pkg_rule_module_needs(f MODULE ergo_render ENABLED TRUE NEEDS pictor
    AVAILABLE "" MODE selected)
pkg_assert_finding("PKG-14 need missing" "${f}" PKG-14 error ergo_render)
ergo_pkg_rule_module_needs(f MODULE ergo_render ENABLED TRUE NEEDS pictor
    AVAILABLE pictor NEED_KIND real-path REAL_PATH FALSE
    REASON "pictor built without PICTOR_HAS_VULKAN" MODE selected)
pkg_assert_finding("PKG-14 real path missing" "${f}" PKG-14 error ergo_render)
ergo_pkg_rule_module_needs(f MODULE ergo_render ENABLED TRUE NEEDS pictor
    AVAILABLE "" MODE M0)
pkg_assert_finding("PKG-14 M0 only records" "${f}" PKG-14 record ergo_render)
pkg_assert_no_finding("PKG-14 M0 not an error" "${f}" "PKG-14\\|error")
ergo_pkg_rule_module_needs(f MODULE ergo_render_viz ENABLED TRUE NEEDS
    AVAILABLE "" MODE selected)
pkg_assert_empty("PKG-14 optional only" "${f}")
pkg_pass("PKG-14")

# --- PKG-15 collisions -----------------------------------------------------
ergo_pkg_rule_collisions(f STRICT OFF
    HOST_CACHE PICTOR_BUILD_DEMO=OFF WIRING_CACHE PICTOR_BUILD_DEMO=OFF
    PROVIDED pictor:pictor tela:Tela::Core tela:Tela::Graph
    BASE_NAMES pictor tela EXTRA_NAMES secret_pkg)
pkg_assert_empty("PKG-15 no collision" "${f}")
ergo_pkg_rule_collisions(f STRICT OFF
    HOST_CACHE PICTOR_BUILD_DEMO=ON WIRING_CACHE PICTOR_BUILD_DEMO=OFF)
pkg_assert_finding("PKG-15 cache conflict" "${f}" PKG-15 warning PICTOR_BUILD_DEMO)
ergo_pkg_rule_collisions(f STRICT ON
    HOST_CACHE PICTOR_BUILD_DEMO=ON WIRING_CACHE PICTOR_BUILD_DEMO=OFF)
pkg_assert_finding("PKG-15 cache conflict strict" "${f}" PKG-15 error PICTOR_BUILD_DEMO)
ergo_pkg_rule_collisions(f STRICT OFF PROVIDED pictor:pictor fake:pictor)
pkg_assert_finding("PKG-15 target twice" "${f}" PKG-15 warning pictor)
ergo_pkg_rule_collisions(f STRICT OFF PROVIDED pictor:pictor pictor:pictor)
pkg_assert_empty("PKG-15 same package repeats its target" "${f}")
ergo_pkg_rule_collisions(f STRICT OFF BASE_NAMES pictor tela EXTRA_NAMES tela)
pkg_assert_finding("PKG-15 extra manifest name" "${f}" PKG-15 error tela)
pkg_pass("PKG-15")

# --- finding helpers -------------------------------------------------------
set(mixed "PKG-01|error|a|x" "PKG-06|warning|b|y" "PKG-14|record|c|z")
ergo_pkg_findings_with_severity(errors error ${mixed})
pkg_assert_eq("severity filter" "${errors}" "PKG-01|error|a|x")
set(msg "")
ergo_pkg_add_finding(msg PKG-02 error p "a;b|c")
pkg_assert_eq("finding message sanitised" "${msg}" "PKG-02|error|p|a,b/c")
pkg_pass("finding helpers")
