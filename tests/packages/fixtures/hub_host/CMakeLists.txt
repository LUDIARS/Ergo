# Fake host for tests/packages/test_hub_routes.cmake. Optionally creates the
# fake package "alpha" itself (FIXTURE_HOST_ALPHA), includes the package hub,
# then writes what a host can observe into hub_facts.txt.
#   FIXTURE_HOST_ALPHA  "" (hub resolves) / before (add_subdirectory before
#                       Ergo: host route) / late (after Ergo, with
#                       ERGO_PACKAGE_PROVIDED_ALPHA) / bad (a bare `alpha`
#                       target without the contract definition)
#   FIXTURE_ALPHA_SRC   source used by before / late
cmake_minimum_required(VERSION 3.20)
project(ergo_pkg_hub_host NONE)

if(FIXTURE_HOST_ALPHA STREQUAL "before")
    add_subdirectory("${FIXTURE_ALPHA_SRC}" host_alpha)
elseif(FIXTURE_HOST_ALPHA STREQUAL "bad")
    add_library(alpha INTERFACE)
endif()

include("${ERGO_SOURCE_DIR}/cmake/packages/ErgoPackages.cmake")

if(FIXTURE_HOST_ALPHA STREQUAL "late")
    add_subdirectory("${FIXTURE_ALPHA_SRC}" host_alpha)
endif()

get_property(_seen GLOBAL PROPERTY FAKE_ALPHA_SEEN)
set(_targets "")
foreach(_t alpha alpha_web Beta::Core gamma)
    if(TARGET ${_t})
        list(APPEND _targets ${_t})
    endif()
endforeach()
string(REPLACE ";" "," _seen "${_seen}")
string(REPLACE ";" "," _targets "${_targets}")
# ALPHA_NO_GPU / ALPHA_BUILD_WEB are wiring-scope variables: they must not
# reach the host (leak= stays empty).
file(WRITE "${CMAKE_BINARY_DIR}/hub_facts.txt"
    "asset_dir=${ALPHA_ASSET_DIR}\nseen=${_seen}\ntargets=${_targets}\nleak=${ALPHA_NO_GPU}${ALPHA_BUILD_WEB}\n")

# Runs after the hub's deferred finalize: reaching it means configure passed.
function(hub_fixture_done)
    file(WRITE "${CMAKE_BINARY_DIR}/hub_fixture_ok.txt" "ok\n")
endfunction()
cmake_language(DEFER CALL hub_fixture_done)
