# Fake package "Beta" (source form). The installed form for the find_package
# route lives in ../beta_install.
cmake_minimum_required(VERSION 3.20)
project(FakeBeta VERSION 0.3.0 LANGUAGES NONE)
add_library(beta_core INTERFACE)
add_library(Beta::Core ALIAS beta_core)
