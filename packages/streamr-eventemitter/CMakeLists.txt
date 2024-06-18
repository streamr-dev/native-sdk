cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 26)

set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES Off)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# make the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(streamr-eventemitter CXX)

message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
      
add_library(streamr-eventemitter INTERFACE)
add_library(streamr::streamr-eventemitter ALIAS streamr-eventemitter)

target_include_directories(
  streamr-eventemitter INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

# TODO: Is there a better way to do this?
# - Apparently not: https://gitlab.kitware.com/cmake/cmake/-/issues/20511
# - At least parse the deps from the vcpkg.eventemitter file and use them to find_package() in this file and to add the
# find_package() calls to the wrapper

export(TARGETS streamr-eventemitter
  NAMESPACE streamr::
  FILE streamr-eventemitter-config-in.cmake)

file(WRITE "${CMAKE_BINARY_DIR}/streamr-eventemitter-config.cmake" 
  "list(APPEND CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})\n"
  "include(${CMAKE_BINARY_DIR}/streamr-eventemitter-config-in.cmake)\n")

enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(streamr-eventemitter-test-unit test/unit/EventEmitterTest.cpp)
target_link_libraries(streamr-eventemitter-test-unit 
  PUBLIC streamr-eventemitter
  PUBLIC GTest::gtest 
  PUBLIC GTest::gtest_main 
  PUBLIC GTest::gmock 
  PUBLIC GTest::gmock_main)

include(GoogleTest)
gtest_discover_tests(streamr-eventemitter-test-unit)
