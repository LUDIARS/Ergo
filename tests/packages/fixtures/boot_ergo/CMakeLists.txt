# Fake Ergo for tests/packages/test_bootstrap.cmake: records which checkout
# the host's bootstrap added (the test writes name.txt next to this file).
cmake_minimum_required(VERSION 3.20)
project(fake_ergo NONE)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/name.txt" _name)
string(STRIP "${_name}" _name)
set_property(GLOBAL PROPERTY FAKE_ERGO_NAME "${_name}")
