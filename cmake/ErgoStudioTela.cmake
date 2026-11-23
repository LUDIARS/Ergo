# ---------------------------------------------------------------------------
# ErgoStudioTela.cmake — Tela for Ergo Studio's UI (spec/feature/tool/shell.md)
#
# ERGO_TELA_SOURCE_DIR = a Tela checkout, added here with add_subdirectory (as
# Odeum is for remote play). Empty = no Tela: the Studio keeps the built-in
# ui_kit window and has no /shell/ (the build and every test still work).
#
# Tela is C++20 (its public headers use defaulted comparisons and designated
# initializers), Ergo is C++17: only the targets that include Tela headers
# (ergo_studio_tela*, ergo_studio, test_studio_tela) are C++20, through
# Tela's own PUBLIC cxx_std_20.
#
# GPU (Tela::Gpu) when the Studio can open a window (pictor + glfw + Vulkan).
# Tela's Pictor adapters take Pictor as a file (find_path / find_library), not
# as the `pictor` target this build makes, so the pictor archive is put in a
# known directory (the same for every configuration) and handed to Tela as that file, built before Tela's GPU
# targets. A Tela that already exists as a target (the package hub with
# ERGO_PACKAGES=tela) is used as is: headless, without Tela::Gpu.
#
# Sets in the caller's scope:
#   ERGO_STUDIO_TELA_WEB_JS  path of Tela's web/tela.js ("" = no Tela)
# ---------------------------------------------------------------------------
function(ergo_studio_add_tela)
    set(ERGO_TELA_SOURCE_DIR "" CACHE PATH "Tela checkout for Ergo Studio's UI (empty = the built-in ui_kit window, no /shell/)")
    set(ERGO_STUDIO_TELA_WEB_JS "" PARENT_SCOPE)
    if(TARGET Tela::Core)
        get_target_property(_tela_src tela_core SOURCE_DIR)
        if(TARGET Tela::Web AND EXISTS "${_tela_src}/web/tela.js")
            set(ERGO_STUDIO_TELA_WEB_JS "${_tela_src}/web/tela.js" PARENT_SCOPE)
            message(STATUS "ergo_studio: Tela from the package hub (${_tela_src}), no GPU backend")
        endif()
        return()
    endif()
    if(NOT ERGO_TELA_SOURCE_DIR)
        message(STATUS "ergo_studio: ERGO_TELA_SOURCE_DIR not set — built-in ui_kit window, no browser shell")
        return()
    endif()
    if(NOT EXISTS "${ERGO_TELA_SOURCE_DIR}/CMakeLists.txt" OR NOT EXISTS "${ERGO_TELA_SOURCE_DIR}/web/tela.js")
        message(FATAL_ERROR "ERGO_TELA_SOURCE_DIR=${ERGO_TELA_SOURCE_DIR} is not a Tela checkout (CMakeLists.txt, web/tela.js)")
    endif()

    set(_gpu OFF)
    if(TARGET pictor AND TARGET glfw)
        find_package(Vulkan QUIET)
        if(Vulkan_FOUND)
            set(_gpu ON)
        endif()
    endif()

    set(TELA_BUILD_WINDOWS OFF CACHE BOOL "" FORCE)
    set(TELA_BUILD_MACOS OFF CACHE BOOL "" FORCE)
    set(TELA_BUILD_PICTOR OFF CACHE BOOL "" FORCE)
    set(TELA_BUILD_GPU ${_gpu} CACHE BOOL "" FORCE)
    if(_gpu)
        get_target_property(_pictor_src pictor SOURCE_DIR)
        get_target_property(_pictor_name pictor OUTPUT_NAME)
        if(NOT _pictor_name)
            set(_pictor_name pictor)
        endif()
        set(_dir "${CMAKE_BINARY_DIR}/_tela_pictor")
        # One directory for every configuration: Tela's imported Pictor has one location (the
        # configuration built last is the one there).
        set_target_properties(pictor PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${_dir}")
        foreach(_cfg ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
            string(TOUPPER "${_cfg}" _up)
            set_target_properties(pictor PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${_up} "${_dir}")
        endforeach()
        set(_file "${CMAKE_STATIC_LIBRARY_PREFIX}${_pictor_name}${CMAKE_STATIC_LIBRARY_SUFFIX}")
        set(TELA_PICTOR_INCLUDE_DIR "${_pictor_src}/include" CACHE PATH "" FORCE)
        set(TELA_PICTOR_LIBRARY "${_dir}/${_file}" CACHE FILEPATH "" FORCE)
        # Only Tela's GLFW check host uses these; the name resolves to this build's glfw target.
        get_target_property(_glfw_src glfw SOURCE_DIR)
        set(TELA_GLFW_INCLUDE_DIR "${_glfw_src}/include" CACHE PATH "" FORCE)
        set(TELA_GLFW_LIBRARY glfw CACHE STRING "" FORCE)
    endif()

    set(_testing ${BUILD_TESTING})
    set(BUILD_TESTING OFF)   # Tela's own tests run in Tela
    add_subdirectory("${ERGO_TELA_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_tela" EXCLUDE_FROM_ALL)
    set(BUILD_TESTING ${_testing})

    if(_gpu AND TARGET tela_gpu)
        add_dependencies(tela_pictor pictor)
        add_dependencies(tela_gpu pictor)
        message(STATUS "ergo_studio: Tela from ${ERGO_TELA_SOURCE_DIR} with the GPU backend (Tela::Gpu)")
    else()
        message(STATUS "ergo_studio: Tela from ${ERGO_TELA_SOURCE_DIR}, no GPU backend (needs pictor + glfw + Vulkan)")
    endif()
    set(ERGO_STUDIO_TELA_WEB_JS "${ERGO_TELA_SOURCE_DIR}/web/tela.js" PARENT_SCOPE)
endfunction()
