cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 23)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# make the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(logger CXX)

find_package(folly REQUIRED)

add_library(streamr-logger INTERFACE)
set_property(TARGET streamr-logger PROPERTY CXX_STANDARD 23)

add_library(streamr::streamr-logger ALIAS streamr-logger)

target_include_directories(
  streamr-logger INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

target_link_libraries(streamr-logger INTERFACE Folly::folly)

# TODO: Is there a better way to do this?
# - Apparently not: https://gitlab.kitware.com/cmake/cmake/-/issues/20511
# - At least parse the deps from the vcpkg.json file and use them to find_package() in this file and to add the
# find_package() calls to the wrapper

export(TARGETS streamr-logger FILE streamr-logger-config-in.cmake)
file(WRITE "${CMAKE_BINARY_DIR}/streamr-logger-config.cmake" "find_package(folly REQUIRED)\ninclude(${CMAKE_BINARY_DIR}/streamr-logger-config-in.cmake)\n")

