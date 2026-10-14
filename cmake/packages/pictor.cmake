# ---------------------------------------------------------------------------
# pictor.cmake — wiring of the Pictor package (manifest entry "pictor")
#
# Included by the package hub (ErgoPackageResolve.cmake) inside a function;
# see ErgoPackageWiring.cmake for the variables in scope. Everything that is
# specific to Pictor's build options lives here, so multi-RHI renames touch
# this file only (spec/feature/package-hub.md §8).
#
# Feature mapping:
#   vulkan  Pictor has no switch for it: it calls find_package(Vulkan) on
#           desktop and defines PICTOR_HAS_VULKAN when found. With vulkan
#           deselected, Vulkan is hidden from Pictor's subtree only
#           (CMAKE_DISABLE_FIND_PACKAGE_Vulkan in this scope); Ergo's own
#           find_package(Vulkan) in ergo_render is unaffected. Android sets
#           Vulkan_FOUND itself, so the switch has no effect there.
#   web     PICTOR_BUILD_WEBGL=ON through the manifest's feature `cache`.
#   webgpu / d3d12 / metal are reserved in the manifest (not selectable).
# ---------------------------------------------------------------------------
if(NOT "vulkan" IN_LIST ERGO_PKG_FEATURES)
    set(CMAKE_DISABLE_FIND_PACKAGE_Vulkan TRUE)
endif()

ergo_pkg_wire_default()
