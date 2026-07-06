cmake_minimum_required(VERSION 3.28...4.3)
include(homebrewClang.cmake)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Module scanning stays OFF globally so that non-module TUs keep clean
# compile commands for clangd; targets with module units or import-using
# sources opt back in via the StreamrModules.cmake helpers.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES Off)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# make the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)
set(VCPKG_TRACE_FIND_PACKAGE TRUE)
set(CMAKE_FIND_USE_CMAKE_PATH ON CACHE BOOL "Force CMake to use CMAKE_PATH" FORCE)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_FIND_USE_PACKAGE_ROOT_PATH ON)
project(logger CXX)

set(CMAKE_FIND_USE_PACKAGE_ROOT_PATH ON)
find_package(folly CONFIG REQUIRED)
# Sibling monorepo packages: find_package only when the target is not
# already in this build tree (in the root single-tree build the sibling
# was added via add_subdirectory — the guard makes the root tree
# self-contained, so it configures without the standalone per-package
# build dirs; MODERNIZATION.md "After the consolidation: CI speed").
if(NOT TARGET streamr::streamr-json)
    find_package(streamr-json CONFIG REQUIRED)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ modules support (guards, policies, helpers) — must come after
# project() because it inspects the compiler id.
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamrModules.cmake)

# CONSOLIDATED (MODERNIZATION.md Phase 2.6): one named sub-module per
# former header (settled architecture: no umbrella; consumers and tests
# import the individual sub-modules — external consumers virtually always
# just streamr.logger.SLogger). The include/ tree is gone; the module
# interface units are the source of truth.
file(GLOB_RECURSE STREAMR_LOGGER_MODULE_UNITS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/*.cppm)
streamr_add_module_library(streamr-logger
  FILES ${STREAMR_LOGGER_MODULE_UNITS})
set_property(TARGET streamr-logger PROPERTY CXX_STANDARD 26)

add_library(streamr::streamr-logger ALIAS streamr-logger)

# Test/example targets import the modules — skip them where modules are
# unsupported (Android; they are never executed there anyway).
if(NOT IOS AND STREAMR_MODULES_SUPPORTED)
  add_executable(env_category_tests test/unit/LoggerEnvTest.cpp)
  streamr_enable_imports(env_category_tests)
  target_link_libraries(env_category_tests PRIVATE streamr-logger)
  enable_testing()
  add_test(
    NAME env_category_fatal_and_info_log_test
    COMMAND "${CMAKE_CURRENT_LIST_DIR}/build/env_category_tests" 1
  )
  set_property(TEST env_category_fatal_and_info_log_test PROPERTY ENVIRONMENT "LOG_LEVEL_test=fatal")

  add_test(
    NAME env_category_fatal_and_fatal_log_test
    COMMAND "${CMAKE_CURRENT_LIST_DIR}/build/env_category_tests" 2
  )
  set_property(TEST env_category_fatal_and_fatal_log_test PROPERTY ENVIRONMENT "LOG_LEVEL_test=fatal")

  add_test(
    NAME env_category_fatal_and_error_log_test
    COMMAND "${CMAKE_CURRENT_LIST_DIR}/build/env_category_tests" 3
  )
  set_property(TEST env_category_fatal_and_error_log_test PROPERTY ENVIRONMENT "LOG_LEVEL_test=fatal")

  add_test(
    NAME env_category_wrongly_written_as_fatal_and_info_log_test
    COMMAND "${CMAKE_CURRENT_LIST_DIR}/build/env_category_tests" 4
  )
  set_property(TEST env_category_wrongly_written_as_fatal_and_info_log_test PROPERTY ENVIRONMENT "LOG_LEVEL=\"\"")
endif()

# PUBLIC (was INTERFACE): the module interface units compile against folly
# and streamr-json in their global module fragments.
target_link_libraries(streamr-logger PUBLIC Folly::folly PUBLIC streamr::streamr-json)
# TODO: Is there a better way to do this?
# - Apparently not: https://gitlab.kitware.com/cmake/cmake/-/issues/20511
# - At least parse the deps from the vcpkg.json file and use them to find_package() in this file and to add the
# find_package() calls to the wrapper

export(TARGETS streamr-logger
  NAMESPACE streamr::
  FILE streamr-logger-config-in.cmake
  CXX_MODULES_DIRECTORY streamr-logger-modules)

  file(WRITE "${CMAKE_BINARY_DIR}/streamr-logger-config.cmake" 
  "list(APPEND CMAKE_FIND_ROOT_PATH \"${CMAKE_FIND_ROOT_PATH}\")\n"
  "find_package(folly REQUIRED)\n"
  "find_package(streamr-json CONFIG REQUIRED)\n"
  "include(${CMAKE_BINARY_DIR}/streamr-logger-config-in.cmake)\n")

# Test/example targets import the modules — skip them where modules are
# unsupported (Android; they are never executed there anyway).
if(NOT IOS AND STREAMR_MODULES_SUPPORTED)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)

  add_executable(streamr-logger-test-unit test/unit/LoggerTest.cpp test/unit/StreamrLogFormatterTest.cpp)
  # LoggerTest imports streamr.logger (StreamrLogFormatterTest keeps
  # #include — it tests a detail/ header that has no partition).
  streamr_enable_imports(streamr-logger-test-unit)

  target_link_libraries(streamr-logger-test-unit
    PRIVATE streamr-logger
    PRIVATE GTest::gtest 
    PRIVATE GTest::gtest_main 
    PRIVATE GTest::gmock 
    PRIVATE GTest::gmock_main)

  if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
    include(GoogleTest)
    gtest_discover_tests(streamr-logger-test-unit)
  endif()

  add_executable(streamr-logger-example src/examples/LoggerExample.cpp)
  streamr_enable_imports(streamr-logger-example)
  target_link_libraries(streamr-logger-example PRIVATE streamr-logger)
endif()