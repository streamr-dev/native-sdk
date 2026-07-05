cmake_minimum_required(VERSION 3.28...4.3)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
include(homebrewClang.cmake)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Module scanning stays OFF globally so that non-module TUs keep clean
# compile commands for clangd; targets with module units or import-using
# sources opt back in via the StreamrModules.cmake helpers.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES Off)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "VCPKG_TARGET_TRIPLET: ${VCPKG_TARGET_TRIPLET}")
message(STATUS "VCPKG_CHAINLOAD_TOOLCHAIN_FILE: ${VCPKG_CHAINLOAD_TOOLCHAIN_FILE}")
set(ENV{VCPKG_TARGET_TRIPLET} ${VCPKG_TARGET_TRIPLET})
# make the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)

#set(TARGET_ARCHITECTURE arm64)
#set(CMAKE_SYSTEM_NAME Android)
#set(CMAKE_SYSTEM_PROCESSOR aarch64)
#set(CMAKE_SYSTEM_NAME Android)
#set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
#set(ANDROID_ABI arm64-v8a)
#set(ANDROID_PLATFORM 24)
#set(ANDROID_NATIVE_API_LEVEL 24)
#set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(streamr-json CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED)
message(STATUS "CMAKE_FIND_ROOT_PATH: ${CMAKE_FIND_ROOT_PATH}")
find_package(nlohmann_json CONFIG REQUIRED)
      

if(NOT TARGET Boost::pfr)
  add_library(Boost::pfr INTERFACE IMPORTED)
  set_target_properties(Boost::pfr PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
endif()

# C++ modules support (guards, policies, helpers) — must come after
# project() because it inspects the compiler id.
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamrModules.cmake)

# CONSOLIDATED (MODERNIZATION.md Phase 2.6): one named sub-module per
# former public header (settled architecture: no umbrella). The include/
# tree is gone; the module interface units are the source of truth, so
# the package no longer exports any include directory.
file(GLOB_RECURSE STREAMR_JSON_MODULE_UNITS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/*.cppm)
streamr_add_module_library(streamr-json
  FILES ${STREAMR_JSON_MODULE_UNITS})
add_library(streamr::streamr-json ALIAS streamr-json)

# PUBLIC (was INTERFACE): the module interface units themselves compile
# against boost-pfr/nlohmann in their global module fragments.
target_link_libraries(streamr-json PUBLIC Boost::pfr)
target_link_libraries(streamr-json PUBLIC nlohmann_json::nlohmann_json)

# Test/example targets import the modules — skip them where modules are
# unsupported (Android; they are never executed there anyway).
if(NOT IOS AND STREAMR_MODULES_SUPPORTED)
  enable_testing()

  add_executable(streamr-json-test-unit test/unit/TestJsonConcepts.cpp test/unit/toStringTest.cpp test/unit/toJsonTest.cpp)
  # The tests import streamr.json, so their sources need module dependency
  # scanning.
  streamr_enable_imports(streamr-json-test-unit)
  target_link_libraries(streamr-json-test-unit
    PRIVATE streamr-json
    PRIVATE GTest::gtest 
    PRIVATE GTest::gtest_main 
    PRIVATE GTest::gmock 
    PRIVATE GTest::gmock_main)

  if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
    include(GoogleTest)
    gtest_discover_tests(streamr-json-test-unit)
  endif()
  
  add_executable(streamr-json-example src/examples/JsonExample.cpp)
  streamr_enable_imports(streamr-json-example)
  target_link_libraries(streamr-json-example PRIVATE streamr-json)

endif()

# TODO: Is there a better way to do this?
# - Apparently not: https://gitlab.kitware.com/cmake/cmake/-/issues/20511
# - At least parse the deps from the vcpkg.json file and use them to find_package() in this file and to add the
# find_package() calls to the wrapper

# CXX_MODULES_DIRECTORY: the export now also carries the C++ module
# interface information so that a standalone downstream build can compile
# BMIs from this build tree (Phase 2.1 smoke-test gate).
export(TARGETS streamr-json
  NAMESPACE streamr::
  FILE streamr-json-config-in.cmake
  CXX_MODULES_DIRECTORY streamr-json-modules)

file(WRITE "${CMAKE_BINARY_DIR}/streamr-json-config.cmake" 
  "list(APPEND CMAKE_FIND_ROOT_PATH \"${CMAKE_FIND_ROOT_PATH}\")\n"
  "find_package(Boost CONFIG REQUIRED)\n"
  "message(STATUS \"CMAKE_FIND_ROOT_PATH: \${CMAKE_FIND_ROOT_PATH}\")\n"
  "find_package(nlohmann_json CONFIG REQUIRED)\n"
  "message(STATUS \"after trying to find lohmann\")\n"
  "if(NOT TARGET Boost::pfr)\n"
    "    add_library(Boost::pfr INTERFACE IMPORTED)\n"
    "    set_target_properties(Boost::pfr PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})\n"
  "endif()\n"
  "include(${CMAKE_BINARY_DIR}/streamr-json-config-in.cmake)\n")
