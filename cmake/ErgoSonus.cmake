# Sonus (LUDIARS/Sonus, C++20 game sound runtime) as a subproject of Ergo.
#
# Sonus is never bundled or fetched: the host points ERGO_SONUS_DIR at a Sonus
# source checkout and Ergo takes it in with add_subdirectory (Sonus
# spec/interface/CONTRACTS.md D-100 / D-101). Used by
#   - ergo_audio's Sonus backend (ERGO_AUDIO_BACKEND=sonus)       -> Sonus::sonus (C API, C++17 friendly)
#   - Ergo Studio's sonus plugin (when Sonus is in the build)     -> Sonus::tool_core (C++20)
#
# Only called when Sonus is wanted, so a build without Sonus keeps Ergo's own
# minimum CMake version (3.20) and toolchains. Every requirement is checked
# here and fails the configure with a message that says what to change; there
# is no silent fallback to another backend.
#
# Sonus applies its warnings, -Werror, -fno-exceptions / -fno-rtti and C++20 to
# its own targets only (snd_apply_options), so Ergo must not put warnings on the
# whole directory (none today): warnings stay per target.

set(ERGO_SONUS_MIN_CMAKE 3.24)

# ergo_sonus_add(TOOL_CORE <ON|OFF>)
#   Adds Sonus once. TOOL_CORE ON also builds Sonus::tool_core (the Studio plugin).
#   Sets ERGO_SONUS_LIVELINK (parent scope) to whether Sonus compiles its live link.
function(ergo_sonus_add)
    cmake_parse_arguments(A "" "TOOL_CORE" "" ${ARGN})
    if(TARGET Sonus::sonus)
        message(FATAL_ERROR "ergo: Sonus was already added to this build (ergo_sonus_add called twice, "
            "or the host added Sonus itself). Let Ergo add it through ERGO_SONUS_DIR only.")
    endif()
    if(CMAKE_VERSION VERSION_LESS ${ERGO_SONUS_MIN_CMAKE})
        message(FATAL_ERROR "ergo: Sonus needs CMake ${ERGO_SONUS_MIN_CMAKE} or newer (this is ${CMAKE_VERSION}). "
            "Use a newer CMake, or build without Sonus (ERGO_AUDIO_BACKEND=auto/fmod/dummy and ERGO_WITH_SONUS=OFF).")
    endif()
    if(MSVC)
        message(FATAL_ERROR "ergo: Sonus does not build with MSVC-compatible compilers (cl / clang-cl; "
            "it uses GCC-style flags and C++20 features cl lacks). Use gcc, clang or MinGW for a Sonus build, "
            "or build without Sonus (ERGO_AUDIO_BACKEND=auto/fmod/dummy and ERGO_WITH_SONUS=OFF).")
    endif()
    if(NOT ERGO_SONUS_DIR)
        message(FATAL_ERROR "ergo: Sonus is requested but ERGO_SONUS_DIR is not set. "
            "Pass -DERGO_SONUS_DIR=<path to a Sonus source checkout> (Ergo never fetches or bundles Sonus).")
    endif()
    get_filename_component(_dir "${ERGO_SONUS_DIR}" ABSOLUTE)
    if(NOT EXISTS "${_dir}/CMakeLists.txt")
        message(FATAL_ERROR "ergo: ERGO_SONUS_DIR='${ERGO_SONUS_DIR}' has no CMakeLists.txt (not a Sonus checkout).")
    endif()
    file(STRINGS "${_dir}/CMakeLists.txt" _project REGEX "^project\\(sonus")
    if(NOT _project)
        message(FATAL_ERROR "ergo: ERGO_SONUS_DIR='${ERGO_SONUS_DIR}' is not the Sonus project "
            "(its CMakeLists.txt does not declare project(sonus ...)).")
    endif()

    # Sonus' own options are cache options; a normal variable set here wins (CMP0077,
    # Sonus requires 3.24) and stays in this function's scope.
    set(SND_BUILD_TOOL_CORE ${A_TOOL_CORE})
    add_subdirectory("${_dir}" "${CMAKE_BINARY_DIR}/_sonus" EXCLUDE_FROM_ALL)

    foreach(_t Sonus::sonus)
        if(NOT TARGET ${_t})
            message(FATAL_ERROR "ergo: the Sonus at '${_dir}' has no ${_t} target. "
                "Ergo needs a Sonus with CONTRACTS.md D-100 (subproject embedding).")
        endif()
    endforeach()
    if(A_TOOL_CORE AND NOT TARGET Sonus::tool_core)
        message(FATAL_ERROR "ergo: the Sonus at '${_dir}' has no Sonus::tool_core target. "
            "The Studio sonus plugin needs a Sonus with CONTRACTS.md D-101 (SND_BUILD_TOOL_CORE).")
    endif()
    # Whether the runtime has a live link (Sonus option SND_LIVELINK, default ON).
    if(DEFINED SND_LIVELINK)
        set(_ll ${SND_LIVELINK})
    else()
        set(_ll ON)
    endif()
    set(ERGO_SONUS_LIVELINK ${_ll} PARENT_SCOPE)
    message(STATUS "ergo: Sonus from ${_dir} (tool core: ${A_TOOL_CORE}, live link: ${_ll})")
endfunction()
