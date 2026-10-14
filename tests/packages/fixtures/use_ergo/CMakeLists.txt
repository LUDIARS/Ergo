# Fake Ergo for tests/packages/test_host_use.cmake. The test copies Ergo's
# cmake/ next to this file, writes a fake ergo-packages.json and commits the
# directory, so the package hub sees a git checkout as Ergo's root (the
# Ergo pin of the host declaration is checked against it).
cmake_minimum_required(VERSION 3.20)
project(fake_ergo_use NONE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/packages/ErgoPackages.cmake")
