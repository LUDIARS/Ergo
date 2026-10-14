# Wiring of the fake package "alpha" (mirrors cmake/packages/pictor.cmake):
# the gpu feature has no switch in the package, so deselecting it sets a
# variable the package reads, in the wiring scope only.
if(NOT "gpu" IN_LIST ERGO_PKG_FEATURES)
    set(ALPHA_NO_GPU ON)
endif()
ergo_pkg_wire_default()
