cmake_minimum_required(VERSION 3.22)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(streamr-utils CXX)

#find_package(folly CONFIG REQUIRED)

add_library(streamr-utils INTERFACE)
add_library(streamr::streamr-utils ALIAS streamr-utils)

target_include_directories(
  streamr-utils INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

#target_link_libraries(streamr-utils)

export(TARGETS streamr-utils FILE streamr-utils-config.cmake)
