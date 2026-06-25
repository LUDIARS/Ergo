# ---------------------------------------------------------------------------
# third_party/dependencies.cmake — managed third-party library registry
#
# Single source of truth for which external libraries Ergo can pull in, pinned
# to exact upstream revisions. The generic fetch engine is cmake/ErgoDependencies
# .cmake; per-library wiring is cmake/deps/<name>.cmake. This file only:
#   1. declares every managed dependency (metadata + pin), and
#   2. opt-in builds the ones whose ERGO_WITH_<NAME> switch is ON.
#
# Header-only libs that are still vendored verbatim under third_party/ (earcut,
# nanosvg, gtest) are NOT managed here — this registry is for fetched, compiled
# dependencies. See spec/setup/third-party-deps.md.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)
include(ErgoDependencies)

# --- Declarations (cheap; nothing is fetched here) -------------------------
ergo_declare_dependency(
    NAME       kazmath
    GIT_REPOSITORY https://github.com/Kazade/kazmath.git
    GIT_TAG    48dbc191da47880ea6708b0a7b3c7b69b6352cad   # no upstream tags; pin HEAD commit
    LICENSE    BSD-2-Clause
    HOMEPAGE   https://github.com/Kazade/kazmath
    SUMMARY    "C math library: vec/mat/quaternion/aabb/plane/ray")

ergo_declare_dependency(
    NAME       curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG    curl-8_19_0
    LICENSE    curl
    HOMEPAGE   https://curl.se
    SUMMARY    "Client-side URL transfer library (HTTP/HTTPS/...)")

# --- Opt-in build switches -------------------------------------------------
# Default OFF: these pull (and, for curl, compile) source from the network, so a
# plain Ergo build stays light and offline-friendly. Turn on the ones a module
# or host actually needs, or flip ERGO_BUILD_THIRDPARTY_SMOKE to verify both.
option(ERGO_WITH_KAZMATH "Fetch + build the managed kazmath library" OFF)
option(ERGO_WITH_CURL    "Fetch + build the managed libcurl library" OFF)
option(ERGO_BUILD_THIRDPARTY_SMOKE
    "Build the third-party smoke test (forces kazmath + curl on)" OFF)

if(ERGO_BUILD_THIRDPARTY_SMOKE)
    set(ERGO_WITH_KAZMATH ON)
    set(ERGO_WITH_CURL    ON)
endif()

if(ERGO_WITH_KAZMATH)
    include(deps/kazmath)
endif()
if(ERGO_WITH_CURL)
    include(deps/curl)
endif()

# --- Umbrella interface ----------------------------------------------------
# `ergo_thirdparty` links whichever managed libs are enabled, so a consumer can
# depend on the set without caring which switches are on.
add_library(ergo_thirdparty INTERFACE)
add_library(ergo::thirdparty ALIAS ergo_thirdparty)
if(TARGET kazmath)
    target_link_libraries(ergo_thirdparty INTERFACE kazmath)
endif()
if(TARGET CURL::libcurl)
    target_link_libraries(ergo_thirdparty INTERFACE CURL::libcurl)
endif()
