cmake_minimum_required(VERSION 3.24)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(streamr-utils CXX)

find_package(folly REQUIRED)

add_library(utils INTERFACE)
add_library(streamr::utils ALIAS utils)

target_include_directories(
  utils INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

target_link_libraries(utils INTERFACE Folly::folly)

export(TARGETS utils FILE utilsConfig.cmake)
