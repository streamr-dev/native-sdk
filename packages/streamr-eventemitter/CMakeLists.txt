cmake_minimum_required(VERSION 3.28...4.3)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Module scanning stays OFF globally so that non-module TUs keep clean
# compile commands for clangd; targets with module units or import-using
# sources opt back in via the StreamrModules.cmake helpers.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
include(homebrewClang.cmake)

set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES Off)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# make the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(streamr-eventemitter CXX)

message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

# C++ modules support (guards, policies, helpers) — must come after
# project() because it inspects the compiler id.
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamrModules.cmake)

# Module façade (MODERNIZATION.md Part 2): the package is now a STATIC
# library whose module interface units re-export the public headers.
# #include consumers are unaffected; import consumers get the BMIs.
streamr_add_module_library(streamr-eventemitter
  FILES
    modules/streamr.eventemitter.cppm
    modules/streamr.eventemitter-EventEmitter.cppm)
add_library(streamr::streamr-eventemitter ALIAS streamr-eventemitter)

target_include_directories(
  streamr-eventemitter PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

# TODO: Is there a better way to do this?
# - Apparently not: https://gitlab.kitware.com/cmake/cmake/-/issues/20511
# - At least parse the deps from the vcpkg.eventemitter file and use them to find_package() in this file and to add the
# find_package() calls to the wrapper

# CXX_MODULES_DIRECTORY: the export now also carries the C++ module
# interface information so that a standalone downstream build can compile
# BMIs from this build tree (Phase 2.1 smoke-test gate).
export(TARGETS streamr-eventemitter
  NAMESPACE streamr::
  FILE streamr-eventemitter-config-in.cmake
  CXX_MODULES_DIRECTORY streamr-eventemitter-modules)

file(WRITE "${CMAKE_BINARY_DIR}/streamr-eventemitter-config.cmake" 
  "list(APPEND CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})\n"
  "include(${CMAKE_BINARY_DIR}/streamr-eventemitter-config-in.cmake)\n")
# Test/example targets import the modules — skip them where modules are
# unsupported (Android; they are never executed there anyway).
if(NOT IOS AND STREAMR_MODULES_SUPPORTED)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)

  add_executable(streamr-eventemitter-test-unit test/unit/EventEmitterTest.cpp)
  # The test imports streamr.eventemitter, so its sources need module
  # dependency scanning.
  streamr_enable_imports(streamr-eventemitter-test-unit)
  target_link_libraries(streamr-eventemitter-test-unit
    PUBLIC streamr-eventemitter
    PUBLIC GTest::gtest 
    PUBLIC GTest::gtest_main 
    PUBLIC GTest::gmock 
    PUBLIC GTest::gmock_main)

  if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
    include(GoogleTest)
    gtest_discover_tests(streamr-eventemitter-test-unit)
  endif()

endif()