# PKG-13 (+ PKG-11 for feature requires): feature defaults, add/subtract
# requests ("+web;-vulkan") and the combination rule.
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")

# Pictor-like feature table.
set(FEATURES d3d12 metal vulkan web webgpu)
set(DEFAULTS vulkan=auto web=auto)
set(PLATFORMS "vulkan=desktop,android,ios" "web=emscripten" "d3d12=" "metal=" "webgpu=")
set(RESERVED d3d12 metal webgpu)

macro(select platform)
    ergo_pkg_features_select(sel f PACKAGE pictor PLATFORM ${platform}
        FEATURES ${FEATURES} DEFAULTS ${DEFAULTS} PLATFORMS ${PLATFORMS}
        RESERVED ${RESERVED} REQUEST ${ARGN})
endmacro()

# Defaults follow the platform ("auto").
select(desktop)
pkg_assert_eq("desktop default" "${sel}" "vulkan")
pkg_assert_empty("desktop default findings" "${f}")
select(emscripten)
pkg_assert_eq("emscripten default" "${sel}" "web")
select(android)
pkg_assert_eq("android default" "${sel}" "vulkan")

# Add and subtract.
select(desktop -vulkan)
pkg_assert_eq("subtract" "${sel}" "")
select(desktop +web)
pkg_assert_eq("add keeps declared order" "${sel}" "vulkan;web")
select(desktop web)
pkg_assert_eq("bare name adds" "${sel}" "vulkan;web")
select(desktop +web -vulkan)
pkg_assert_eq("add and subtract" "${sel}" "web")
select(desktop -vulkan +vulkan)
pkg_assert_eq("later token wins" "${sel}" "vulkan")
select(desktop -web)
pkg_assert_eq("subtract absent is a no-op" "${sel}" "vulkan")
pkg_assert_empty("subtract absent findings" "${f}")

# Request errors.
select(desktop +vulcan)
pkg_assert_finding("unknown feature" "${f}" PKG-13 error pictor/vulcan)
pkg_assert_eq("unknown feature ignored" "${sel}" "vulkan")
select(desktop "+=x")
pkg_assert_finding("malformed token" "${f}" PKG-13 error pictor)
select(desktop +webgpu)
pkg_assert_finding("reserved selected" "${f}" PKG-13 error pictor/webgpu)
pkg_assert_eq("reserved stays visible" "${sel}" "vulkan;webgpu")

# Explicit on / off defaults and a bad default value.
ergo_pkg_features_select(sel f PACKAGE p PLATFORM desktop FEATURES a b c
    DEFAULTS a=on b=off c=sometimes)
pkg_assert_eq("on/off defaults" "${sel}" "a")
pkg_assert_finding("bad default" "${f}" PKG-13 error p/c)
pkg_pass("feature selection")

# --- combination -----------------------------------------------------------
macro(combine platform)
    ergo_pkg_rule_feature_combination(f PACKAGE pictor PLATFORM ${platform}
        PLATFORMS ${PLATFORMS} RESERVED ${RESERVED} ${ARGN})
endmacro()

combine(desktop SELECTED vulkan REQUIRE_ANY vulkan web)
pkg_assert_empty("valid desktop combination" "${f}")
combine(emscripten SELECTED web)
pkg_assert_empty("valid emscripten combination" "${f}")
combine(emscripten SELECTED vulkan)
pkg_assert_finding("platform mismatch" "${f}" PKG-13 error pictor/vulkan)
combine(desktop SELECTED vulkan metal)
pkg_assert_finding("reserved in combination" "${f}" PKG-13 error pictor/metal)
combine(desktop SELECTED REQUIRE_ANY vulkan web)
pkg_assert_finding("render feature required" "${f}" PKG-13 error pictor)
combine(desktop SELECTED vulkan web CONFLICTS "vulkan!web")
pkg_assert_finding("conflict" "${f}" PKG-13 error pictor/vulkan)
combine(desktop SELECTED vulkan CONFLICTS "vulkan!web")
pkg_assert_empty("conflict not triggered" "${f}")
ergo_pkg_rule_feature_combination(f PACKAGE p PLATFORM desktop
    SELECTED a REQUIRES "a=b")
pkg_assert_finding("feature requires unmet" "${f}" PKG-11 error p/a)
ergo_pkg_rule_feature_combination(f PACKAGE p PLATFORM desktop
    SELECTED a b REQUIRES "a=b")
pkg_assert_empty("feature requires met" "${f}")
pkg_pass("PKG-13 combination")
