# ---------------------------------------------------------------------------
# ErgoStudioPortus.cmake — Portus C++ for Ergo Studio's web server
# (spec/feature/tool/ergo.md "Web サーバ", spec/plan/2026-11-23-studio-launcher-and-devserver.md, B)
#
# ERGO_PORTUS_SOURCE_DIR = the cpp/ folder of a Portus checkout (CMakeLists.txt,
# include/portus/http.hpp), added here with add_subdirectory as Tela and Odeum are.
# Portus needs OpenSSL 3 (find_package in its own CMakeLists.txt).
#
# Without it there is no web server, and the ergo_studio app is not built: the
# Studio does not fall back to its old src/studio/net server. The libraries and
# tests that do not serve HTTP still build. ERGO_STUDIO_REQUIRE_WEB=ON turns a
# missing Portus into a configuration error (a build that must produce the app).
#
# Sets in the caller's scope:
#   ERGO_STUDIO_HAS_PORTUS  ON when the portus target exists
# ---------------------------------------------------------------------------
function(ergo_studio_add_portus)
    set(ERGO_PORTUS_SOURCE_DIR "" CACHE PATH "Portus checkout's cpp/ folder for Ergo Studio's web server (empty = no web server, no ergo_studio app)")
    option(ERGO_STUDIO_REQUIRE_WEB "Fail the configuration when Ergo Studio's web server (Portus) is not available" OFF)
    set(ERGO_STUDIO_HAS_PORTUS OFF PARENT_SCOPE)
    if(TARGET portus)
        set(ERGO_STUDIO_HAS_PORTUS ON PARENT_SCOPE)
        return()
    endif()
    if(NOT ERGO_PORTUS_SOURCE_DIR)
        if(ERGO_STUDIO_REQUIRE_WEB)
            message(FATAL_ERROR "ergo_studio: ERGO_STUDIO_REQUIRE_WEB=ON but ERGO_PORTUS_SOURCE_DIR is not set (the cpp/ folder of a Portus checkout)")
        endif()
        message(STATUS "ergo_studio: ERGO_PORTUS_SOURCE_DIR not set — no web server: the ergo_studio app is not built (no fallback to src/studio/net)")
        return()
    endif()
    if(NOT EXISTS "${ERGO_PORTUS_SOURCE_DIR}/CMakeLists.txt" OR NOT EXISTS "${ERGO_PORTUS_SOURCE_DIR}/include/portus/http.hpp")
        message(FATAL_ERROR "ERGO_PORTUS_SOURCE_DIR=${ERGO_PORTUS_SOURCE_DIR} is not Portus's cpp/ folder (CMakeLists.txt, include/portus/http.hpp)")
    endif()
    set(PORTUS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(PORTUS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${ERGO_PORTUS_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_portus" EXCLUDE_FROM_ALL)
    set(ERGO_STUDIO_HAS_PORTUS ON PARENT_SCOPE)
    message(STATUS "ergo_studio: web server on Portus C++ (${ERGO_PORTUS_SOURCE_DIR})")
endfunction()
