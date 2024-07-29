cmake_minimum_required(VERSION 3.22)
include(homebrewClang.cmake)

set(CMAKE_CXX_STANDARD 26)
#set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES Off)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# make the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(logger CXX)

find_package(folly REQUIRED)
find_package(streamr-json CONFIG REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library(streamr-logger INTERFACE)
set_property(TARGET streamr-logger PROPERTY CXX_STANDARD 26)

add_library(streamr::streamr-logger ALIAS streamr-logger)

add_executable(env_category_tests test/unit/LoggerEnvTest.cpp)
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

target_include_directories(
  streamr-logger INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

target_link_libraries(streamr-logger INTERFACE Folly::folly INTERFACE streamr::streamr-json)

# TODO: Is there a better way to do this?
# - Apparently not: https://gitlab.kitware.com/cmake/cmake/-/issues/20511
# - At least parse the deps from the vcpkg.json file and use them to find_package() in this file and to add the
# find_package() calls to the wrapper

export(TARGETS streamr-logger 
  NAMESPACE streamr::
  FILE streamr-logger-config-in.cmake)

  file(WRITE "${CMAKE_BINARY_DIR}/streamr-logger-config.cmake" 
  "list(APPEND CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})\n"
  "find_package(folly REQUIRED)\n"
  "find_package(streamr-json CONFIG REQUIRED)\n"
  "include(${CMAKE_BINARY_DIR}/streamr-logger-config-in.cmake)\n")

enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(streamr-logger-test-unit test/unit/LoggerTest.cpp test/unit/StreamrLogFormatterTest.cpp)

target_link_libraries(streamr-logger-test-unit 
  PRIVATE streamr-logger
  PRIVATE GTest::gtest 
  PRIVATE GTest::gtest_main 
  PRIVATE GTest::gmock 
  PRIVATE GTest::gmock_main)

include(GoogleTest)
gtest_discover_tests(streamr-logger-test-unit)

add_executable(streamr-logger-example src/examples/LoggerExample.cpp)
target_link_libraries(streamr-logger-example PRIVATE streamr-logger)
