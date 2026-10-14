# Fake host for tests/packages/test_host_configure.cmake. The test copies this
# directory and puts the host declaration (or nothing) next to it as
# ergo-packages.json; the hub then picks it up as the host declaration
# (spec/feature/package-manager.md §2.1). project NONE: no compiler.
cmake_minimum_required(VERSION 3.20)
project(ergo_pkg_decl_host NONE)
include("${ERGO_SOURCE_DIR}/cmake/packages/ErgoPackages.cmake")
