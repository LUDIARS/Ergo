# Fake host for tests/packages/test_bootstrap.cmake: includes the generated
# cmake/ergo-bootstrap.cmake and writes what it resolved to boot_facts.txt.
cmake_minimum_required(VERSION 3.20)
project(fake_host NONE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ergo-bootstrap.cmake")
get_property(_name GLOBAL PROPERTY FAKE_ERGO_NAME)
file(WRITE "${CMAKE_BINARY_DIR}/boot_facts.txt" "route=${ERGO_BOOTSTRAP_ROUTE}\nergo=${_name}\n")
