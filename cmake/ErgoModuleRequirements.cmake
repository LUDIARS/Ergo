# ---------------------------------------------------------------------------
# cmake/ErgoModuleRequirements.cmake — module selection checks + library type
#
# Included once from the top-level CMakeLists.txt, after the package hub and
# before the module sections. It runs in Ergo's directory scope (include, not
# a function), so nothing here reaches the host's scope.
#
#   1. Required modules. A module that links another module and has no code
#      path without it stops the configure when that module is OFF, naming both
#      options (the ergo_resource / ergo_http wording), instead of a later
#      "cannot find -lergo_<x>" at link time.
#   2. Optional parents. Modules that build on top of another module and are
#      simply left out when it is OFF say so (STATUS), as render_viz /
#      graph_layout / package_viz already do.
#   3. Library type. Every module library is STATIC: Ergo has no export macros,
#      so an untyped add_library() must not become a DLL / .so that exports
#      nothing when the host (or the command line) sets BUILD_SHARED_LIBS=ON.
#      Set as a normal variable, so the host's own value is untouched and the
#      hub-wired packages (pictor / tela, added above) still follow it.
# ---------------------------------------------------------------------------

# <module>|<required module>... (option suffixes: ERGO_BUILD_<name>). Every
# violated pair is reported in one message.
function(_ergo_check_module_requirements)
    set(_errors "")
    foreach(_entry "PHYSICS2D|MATH" "UI_KIT|IO" "UI_LAYOUT|IO" "RESOURCE|IO")
        string(REPLACE "|" ";" _entry "${_entry}")
        list(POP_FRONT _entry _module)
        if(NOT ERGO_BUILD_${_module})
            continue()
        endif()
        foreach(_dep IN LISTS _entry)
            if(NOT ERGO_BUILD_${_dep})
                string(TOLOWER "${_module}" _m)
                string(TOLOWER "${_dep}" _d)
                string(APPEND _errors
                    "\n  ERGO_BUILD_${_module}=ON but ERGO_BUILD_${_dep}=OFF. ergo_${_m} links "
                    "ergo_${_d} and has no code path without it. Set ERGO_BUILD_${_dep}=ON "
                    "(or ERGO_BUILD_${_module}=OFF).")
            endif()
        endforeach()
    endforeach()
    if(_errors)
        message(FATAL_ERROR "Ergo module selection is incomplete:${_errors}\nNo silent fallback.")
    endif()
endfunction()
_ergo_check_module_requirements()

# Optional parents: the module is skipped (its section checks the same options).
if(ERGO_BUILD_ACTOR AND NOT ERGO_BUILD_BIND)
    message(STATUS "ergo_actor: requires ERGO_BUILD_BIND — skipped")
endif()
if(ERGO_BUILD_CAST AND NOT (ERGO_BUILD_ACTOR AND ERGO_BUILD_BIND))
    message(STATUS "ergo_cast: requires ERGO_BUILD_ACTOR and ERGO_BUILD_BIND — skipped")
endif()
if(ERGO_BUILD_SHURIKEN_MIGRATOR AND NOT (ERGO_BUILD_PARTICLE AND ERGO_BUILD_GPU_PARTICLE))
    message(STATUS "ergo_shuriken_migrator: requires ERGO_BUILD_PARTICLE and "
        "ERGO_BUILD_GPU_PARTICLE — skipped")
endif()

if(BUILD_SHARED_LIBS)
    message(STATUS "ergo: module libraries are always STATIC; BUILD_SHARED_LIBS ignored")
endif()
set(BUILD_SHARED_LIBS OFF)
