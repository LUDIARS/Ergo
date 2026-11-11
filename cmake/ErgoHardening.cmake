# ---------------------------------------------------------------------------
# cmake/ErgoHardening.cmake — Lorica on every target Ergo compiles
#
# Lorica (LUDIARS/Lorica, pinned in third_party/dependencies.cmake) gives each
# locally compiled target:
#   - Lorica::boundary          checked size arithmetic / bounded views / request
#                               budgets (lorica/checked_size.hpp, byte_view.hpp,
#                               input_budget.hpp; the C++20 headers need C++20)
#   - lorica_harden_target      the baseline build profile (stack protector, PIE,
#                               full RELRO, NX, FORTIFY, libstdc++ assertions /
#                               libc++ FAST, stack clash + CET) and, for the
#                               network-facing targets, the EXTENDED profile
#   - lorica_deprecate_unsafe_apis  strcpy / strcat / sprintf / vsprintf /
#                               wcscpy / wcscat in the target's C++ sources fail
#                               to compile (and every deprecation is an error)
#
# Flow (CMakeLists.txt):
#   include(ErgoHardening)                   options + ergo_hardening_mark_network()
#   ergo_hardening_setup()                   after the package hub: decide whether
#                                            Lorica applies here, acquire it
#   ergo_harden_targets()                    last statement: walk Ergo's own
#                                            directories and apply to each target
# A deferred check at the end of the top-level directory (the host's, when Ergo
# is a subproject) fails configuration when an Ergo target is left without it.
#
# Toolchains Lorica's profiles do not cover (macOS / iOS / Android / Emscripten
# / MinGW, or CMake older than Lorica's 3.24) are not silently weakened: with
# ERGO_HARDENING=auto (default) configuration says so with a WARNING and builds
# without Lorica; ERGO_HARDENING=required makes that a configure error.
#
# Hosts that already provide Lorica (add_subdirectory / FetchContent /
# find_package before Ergo) are reused: Ergo adds no second copy.
# See spec/setup/hardening.md.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

set(ERGO_HARDENING "auto" CACHE STRING
    "Lorica on Ergo's targets: auto (apply where Lorica supports the toolchain, WARNING elsewhere) or required (configure error elsewhere)")
set_property(CACHE ERGO_HARDENING PROPERTY STRINGS auto required)
set(ERGO_HARDENING_PROFILE "auto" CACHE STRING
    "Lorica profile of Ergo's network-facing targets: auto (EXTENDED where supported), extended (required) or baseline")
set_property(CACHE ERGO_HARDENING_PROFILE PROPERTY STRINGS auto extended baseline)
if(NOT DEFINED CACHE{LORICA_SOURCE_DIR} AND DEFINED ENV{LORICA_SOURCE_DIR})
    # Nested configures (tests/packages host tests) receive the outer build's
    # Lorica through the environment instead of a network fetch.
    set(LORICA_SOURCE_DIR "$ENV{LORICA_SOURCE_DIR}" CACHE PATH
        "Local Lorica checkout (empty: ERGO_SOURCE_DIR_LORICA or the pinned fetch)")
else()
    set(LORICA_SOURCE_DIR "" CACHE PATH
        "Local Lorica checkout (empty: ERGO_SOURCE_DIR_LORICA or the pinned fetch)")
endif()

if(NOT ERGO_HARDENING MATCHES "^(auto|required)$")
    message(FATAL_ERROR "ERGO_HARDENING must be auto or required, not '${ERGO_HARDENING}'")
endif()
if(NOT ERGO_HARDENING_PROFILE MATCHES "^(auto|extended|baseline)$")
    message(FATAL_ERROR "ERGO_HARDENING_PROFILE must be auto, extended or baseline, not '${ERGO_HARDENING_PROFILE}'")
endif()

# ergo_hardening_mark_network(<target>...): the target takes input from a socket
# for as long as it runs (servers, bridges, protocol clients). It gets the
# EXTENDED profile per ERGO_HARDENING_PROFILE. Targets with per-frame render or
# simulation work stay at the baseline.
function(ergo_hardening_mark_network)
    foreach(_t IN LISTS ARGN)
        set_property(TARGET ${_t} PROPERTY ERGO_NETWORK_BOUNDARY TRUE)
    endforeach()
endfunction()

# Why Lorica's profiles cannot be applied here, or empty when they can.
function(_ergo_hardening_unsupported out)
    set(_r "")
    if(CMAKE_VERSION VERSION_LESS 3.24)
        set(_r "Lorica needs CMake 3.24 or newer (this is ${CMAKE_VERSION})")
    elseif(MSVC AND WIN32)
        set(_r "")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
        set(_r "")
    else()
        set(_r "Lorica's build profiles cover Windows MSVC-compatible and Linux GCC/Clang toolchains only, not ${CMAKE_SYSTEM_NAME} ${CMAKE_CXX_COMPILER_ID}")
    endif()
    set(${out} "${_r}" PARENT_SCOPE)
endfunction()

# Lorica's targets and functions: the host's when it already has them, else
# LORICA_SOURCE_DIR, else the managed dependency (ERGO_SOURCE_DIR_LORICA or the
# pinned fetch; ERGO_FETCH_DEPENDENCIES=OFF without either is a configure error).
function(_ergo_hardening_acquire_lorica)
    if(TARGET Lorica::boundary)
        if(NOT COMMAND lorica_harden_target OR NOT COMMAND lorica_deprecate_unsafe_apis)
            message(FATAL_ERROR
                "ergo: Lorica::boundary exists but lorica_harden_target / lorica_deprecate_unsafe_apis "
                "are not defined. Provide Lorica through add_subdirectory / FetchContent / "
                "find_package(Lorica CONFIG) so its CMake module is loaded. No silent fallback.")
        endif()
        message(STATUS "ergo: using the host's Lorica")
        return()
    endif()
    # Ergo uses only the header-only boundary target; Lorica's protection
    # library and tests stay out of the host's build. Normal variables, so a
    # host's cache is not touched (Lorica's option() honours them: CMP0077).
    set(LORICA_BUILD_PROTECTION OFF)
    set(LORICA_BUILD_TESTS OFF)
    set(LORICA_BUILD_FUZZER OFF)
    if(LORICA_SOURCE_DIR)
        if(NOT EXISTS "${LORICA_SOURCE_DIR}/cmake/LoricaHardening.cmake")
            message(FATAL_ERROR "LORICA_SOURCE_DIR='${LORICA_SOURCE_DIR}' is not a Lorica checkout")
        endif()
        message(STATUS "ergo: Lorica from LORICA_SOURCE_DIR=${LORICA_SOURCE_DIR}")
        add_subdirectory("${LORICA_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_ergo_lorica" EXCLUDE_FROM_ALL)
    else()
        ergo_require_dependency(lorica VERIFY_TARGETS Lorica::boundary)
    endif()
    if(NOT COMMAND lorica_harden_target)
        message(FATAL_ERROR "ergo: Lorica was added but lorica_harden_target is not defined")
    endif()
endfunction()

# ergo_hardening_setup(): call once from Ergo's top-level CMakeLists.txt, after
# the package hub (a hub-acquired package may bring Lorica itself).
function(ergo_hardening_setup)
    _ergo_hardening_unsupported(_reason)
    if(_reason)
        if(ERGO_HARDENING STREQUAL "required")
            message(FATAL_ERROR "ergo: ERGO_HARDENING=required but ${_reason}. No silent fallback.")
        endif()
        message(WARNING
            "ergo: Ergo's targets are built WITHOUT Lorica hardening: ${_reason}. "
            "This build has no stack protector / FORTIFY / RELRO / unsafe-API rejection from Lorica. "
            "Set ERGO_HARDENING=required to make this an error.")
        set_property(GLOBAL PROPERTY ERGO_HARDENING_ACTIVE FALSE)
        set_property(GLOBAL PROPERTY ERGO_HARDENING_REASON "${_reason}")
        return()
    endif()
    _ergo_hardening_acquire_lorica()
    set_property(GLOBAL PROPERTY ERGO_HARDENING_ACTIVE TRUE)

    # EXTENDED for the network-facing targets.
    set(_extended FALSE)
    if(ERGO_HARDENING_PROFILE STREQUAL "extended")
        set(_extended TRUE)
    elseif(ERGO_HARDENING_PROFILE STREQUAL "auto")
        lorica_extended_hardening_supported(_ergo_ext)
        if(_ergo_ext)
            set(_extended TRUE)
        else()
            message(STATUS "ergo: network-facing targets keep Lorica's baseline profile: ${_ergo_ext_REASON}")
        endif()
    endif()
    set_property(GLOBAL PROPERTY ERGO_HARDENING_EXTENDED ${_extended})

    lorica_unsafe_api_rejection_supported(_ergo_unsafe)
    set_property(GLOBAL PROPERTY ERGO_HARDENING_UNSAFE_API ${_ergo_unsafe})
    if(NOT _ergo_unsafe)
        message(STATUS "ergo: unsafe C API rejection not applied with this toolchain: ${_ergo_unsafe_REASON}")
        set_property(GLOBAL PROPERTY ERGO_HARDENING_UNSAFE_API_REASON "${_ergo_unsafe_REASON}")
    endif()

    if(NOT ERGO_POSITION_INDEPENDENT_CODE)
        message(WARNING "ergo: ERGO_POSITION_INDEPENDENT_CODE=OFF is overridden: Lorica builds every hardened target as PIC / PIE")
    endif()
endfunction()

# Ergo's own directories: this one and its subdirectories inside the source tree,
# but not fetched sources under the build tree (curl, Pictor, Lorica, ...).
function(_ergo_hardening_directories dir out)
    get_property(_root GLOBAL PROPERTY ERGO_HARDENING_SOURCE_ROOT)
    set(_dirs "${dir}")
    get_property(_subs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(_s IN LISTS _subs)
        cmake_path(IS_PREFIX _root "${_s}" NORMALIZE _inside)
        set(_in_build FALSE)
        if(NOT CMAKE_BINARY_DIR STREQUAL _root)  # an in-source build holds no fetched trees apart
            cmake_path(IS_PREFIX CMAKE_BINARY_DIR "${_s}" NORMALIZE _in_build)
        endif()
        if(_inside AND NOT _in_build)
            _ergo_hardening_directories("${_s}" _more)
            list(APPEND _dirs ${_more})
        endif()
    endforeach()
    set(${out} "${_dirs}" PARENT_SCOPE)
endfunction()

function(_ergo_hardening_compiled_targets out)
    get_property(_root GLOBAL PROPERTY ERGO_HARDENING_SOURCE_ROOT)
    _ergo_hardening_directories("${_root}" _dirs)
    set(_found "")
    foreach(_d IN LISTS _dirs)
        get_property(_targets DIRECTORY "${_d}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(_t IN LISTS _targets)
            get_target_property(_type ${_t} TYPE)
            if(_type MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
                list(APPEND _found ${_t})
            endif()
        endforeach()
    endforeach()
    set(${out} "${_found}" PARENT_SCOPE)
endfunction()

# True when the target compiles a C source (unsafe API rejection is C++ only).
function(_ergo_hardening_has_c_source target out)
    set(_has FALSE)
    get_target_property(_sources ${target} SOURCES)
    foreach(_s IN LISTS _sources)
        if(_s MATCHES "\\.c$")
            set(_has TRUE)
        endif()
    endforeach()
    set(${out} ${_has} PARENT_SCOPE)
endfunction()

# Fails configuration when one of Ergo's compiled targets has no Lorica
# hardening, or neither the unsafe API rejection nor a recorded reason.
function(ergo_hardening_verify)
    get_property(_active GLOBAL PROPERTY ERGO_HARDENING_ACTIVE)
    if(NOT _active)
        return()
    endif()
    _ergo_hardening_compiled_targets(_targets)
    set(_missing "")
    foreach(_t IN LISTS _targets)
        get_target_property(_hardened ${_t} LORICA_HARDENED)
        get_target_property(_unsafe ${_t} LORICA_UNSAFE_APIS_DEPRECATED)
        get_target_property(_skip ${_t} ERGO_UNSAFE_API_SKIPPED)
        if(NOT _hardened OR (NOT _unsafe AND NOT _skip))
            list(APPEND _missing ${_t})
        endif()
    endforeach()
    if(_missing)
        list(JOIN _missing ", " _list)
        message(FATAL_ERROR
            "ergo: targets without Lorica hardening: ${_list}. Define Ergo targets before "
            "ergo_harden_targets() (the last statement of Ergo's CMakeLists.txt).")
    endif()
endfunction()

# The tests that configure a nested host project (tests/packages, tests/audio,
# tests/thirdparty) get the same Lorica source through LORICA_SOURCE_DIR in
# their environment, so they stay offline.
function(_ergo_hardening_forward_to_tests)
    set(_src "${LORICA_SOURCE_DIR}")
    if(NOT _src)
        get_target_property(_real Lorica::boundary ALIASED_TARGET)
        if(_real)
            get_target_property(_src ${_real} SOURCE_DIR)
        endif()
    endif()
    get_property(_tests DIRECTORY PROPERTY TESTS)  # called from Ergo's top-level directory
    if(_src AND _tests)
        set_property(TEST ${_tests} APPEND PROPERTY ENVIRONMENT "LORICA_SOURCE_DIR=${_src}")
    endif()
endfunction()

# ergo_harden_targets(): the last statement of Ergo's top-level CMakeLists.txt.
function(ergo_harden_targets)
    get_property(_active GLOBAL PROPERTY ERGO_HARDENING_ACTIVE)
    if(NOT _active)
        return()
    endif()
    get_property(_extended GLOBAL PROPERTY ERGO_HARDENING_EXTENDED)
    get_property(_unsafe GLOBAL PROPERTY ERGO_HARDENING_UNSAFE_API)
    get_property(_unsafe_reason GLOBAL PROPERTY ERGO_HARDENING_UNSAFE_API_REASON)
    _ergo_hardening_compiled_targets(_targets)
    set(_n_extended 0)
    set(_c_only "")
    foreach(_t IN LISTS _targets)
        # BUILD_INTERFACE: nothing of Lorica's leaks into an export of Ergo's targets.
        target_link_libraries(${_t} PRIVATE "$<BUILD_INTERFACE:Lorica::boundary>")
        lorica_harden_target(${_t})
        get_target_property(_net ${_t} ERGO_NETWORK_BOUNDARY)
        if(_net AND _extended)
            lorica_harden_target(${_t} EXTENDED)
            math(EXPR _n_extended "${_n_extended} + 1")
        endif()
        _ergo_hardening_has_c_source(${_t} _has_c)
        if(_has_c)
            set_property(TARGET ${_t} PROPERTY ERGO_UNSAFE_API_SKIPPED "C sources (the rejection covers C++ only)")
            list(APPEND _c_only ${_t})
        elseif(_unsafe)
            lorica_deprecate_unsafe_apis(${_t})
        else()
            set_property(TARGET ${_t} PROPERTY ERGO_UNSAFE_API_SKIPPED "${_unsafe_reason}")
        endif()
    endforeach()
    list(LENGTH _targets _n)
    message(STATUS "ergo: Lorica applied to ${_n} target(s) (${_n_extended} EXTENDED)")
    if(_c_only)
        list(JOIN _c_only ", " _c_list)
        message(STATUS "ergo: unsafe C API rejection skipped for C targets: ${_c_list}")
    endif()
    _ergo_hardening_forward_to_tests()
    # Checked once the top-level directory (the host's, as a subproject) is done.
    cmake_language(EVAL CODE
        "cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]] CALL ergo_hardening_verify)")
endfunction()

# A global property: the deferred check runs in the host's top-level scope.
set_property(GLOBAL PROPERTY ERGO_HARDENING_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
