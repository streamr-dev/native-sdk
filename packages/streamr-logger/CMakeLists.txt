cmake_minimum_required(VERSION 3.24)


message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
#set(VCPKG_MANIFEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
#set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)

#set(VCPKG_TRACE_FIND_PACKAGE ON)

# make the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

#unset(VCPKG_TOOLCHAIN)

#set(VCPKG_OVERRIDE_FIND_PACKAGE_NAME "waopijfwpoij")

#z_vcpkg_get_command_underscores(add_library und_count)
#message(STATUS "Underscore count: ${und_count}")

#include($ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

#z_vcpkg_get_command_underscores(add_library und_count2)
#message(STATUS "Underscore count2: ${und_count2}")

message(STATUS "CMake version: ${CMAKE_VERSION}")


#set(VCPKG_MANIFEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

project(streamr-logger CXX)

find_package(folly CONFIG REQUIRED)

add_library(logger INTERFACE)
add_library(streamr::logger ALIAS logger)

target_include_directories(
  logger INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

target_link_libraries(logger INTERFACE Folly::folly)

export(TARGETS logger FILE loggerConfig.cmake)

