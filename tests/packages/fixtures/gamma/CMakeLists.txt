# Fake private package "Gamma", declared only by a host extra manifest
# (ERGO_PACKAGE_MANIFEST_EXTRA).
cmake_minimum_required(VERSION 3.20)
project(FakeGamma VERSION 0.1.0 LANGUAGES NONE)
add_library(gamma INTERFACE)
