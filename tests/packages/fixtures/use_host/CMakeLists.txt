# Fake host for tests/packages/test_host_use.cmake, shaped like
# Ludellus-Native during its migration (spec/feature/package-manager.md §5.2):
# Ergo comes in through the generated cmake/ergo-bootstrap.cmake and the host
# declaration (ergo-packages.json) gives the hub its defaults.
#   FIXTURE_PROVIDED   ON: the host declares alpha provided (L2)
#   FIXTURE_ALPHA_SRC  the host add_subdirectory()s alpha after Ergo, with a
#                      FORCEd cache value the manifest disagrees with (L2)
cmake_minimum_required(VERSION 3.20)
project(fake_use_host NONE)

if(FIXTURE_PROVIDED)
    set(ERGO_PACKAGE_PROVIDED_ALPHA ON)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ergo-bootstrap.cmake")
if(FIXTURE_ALPHA_SRC)
    set(ALPHA_BUILD_DEMO ON CACHE BOOL "Host-forced (PKG-15)" FORCE)
    add_subdirectory("${FIXTURE_ALPHA_SRC}" host_alpha)
endif()

get_property(_seen GLOBAL PROPERTY FAKE_ALPHA_SEEN)
set(_targets "")
foreach(_t alpha alpha_web)
    if(TARGET ${_t})
        list(APPEND _targets ${_t})
    endif()
endforeach()
string(REPLACE ";" "," _seen "${_seen}")
string(REPLACE ";" "," _targets "${_targets}")
file(WRITE "${CMAKE_BINARY_DIR}/use_facts.txt" "seen=${_seen}\ntargets=${_targets}\n")

# Runs after the hub's deferred finalize: reaching it means configure passed.
function(use_fixture_done)
    file(WRITE "${CMAKE_BINARY_DIR}/use_fixture_ok.txt" "ok\n")
endfunction()
cmake_language(DEFER CALL use_fixture_done)
