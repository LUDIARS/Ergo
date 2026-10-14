# ---------------------------------------------------------------------------
# tela.cmake — wiring of the Tela package (manifest entry "tela")
#
# Included by the package hub (ErgoPackageResolve.cmake) inside a function;
# see ErgoPackageWiring.cmake for the variables in scope. Ergo builds Tela
# headless only: the manifest's `cmake.cache` keeps TELA_BUILD_WINDOWS /
# TELA_BUILD_PICTOR OFF (the Windows / Pictor adapters ask for Pictor as a
# file and cannot take the `pictor` target, spec/feature/package-hub.md §1).
# Tela declares no features, so nothing else is mapped.
# ---------------------------------------------------------------------------
ergo_pkg_wire_default()
