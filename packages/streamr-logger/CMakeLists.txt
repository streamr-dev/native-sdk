cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 26)
#set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES Off)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# make the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(logger CXX)

find_package(folly REQUIRED)
find_package(streamr-json REQUIRED)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library(streamr-logger INTERFACE)
set_property(TARGET streamr-logger PROPERTY CXX_STANDARD 23)

add_library(streamr::streamr-logger ALIAS streamr-logger)

target_include_directories(
  streamr-logger INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

target_link_libraries(streamr-logger INTERFACE Folly::folly INTERFACE streamr-json)
#target_link_libraries(streamr-logger INTERFACE streamr-json)
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

#"list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share)\n"
enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(streamr-logger-test-unit test/unit/LoggerTest.cpp test/unit/StreamrLogFormatterTest.cpp)
target_link_libraries(streamr-logger-test-unit 
  PUBLIC streamr-logger
  PUBLIC GTest::gtest 
  PUBLIC GTest::gtest_main 
  PUBLIC GTest::gmock 
  PUBLIC GTest::gmock_main)

include(GoogleTest)
gtest_discover_tests(streamr-logger-test-unit)