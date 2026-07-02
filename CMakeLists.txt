cmake_minimum_required(VERSION 3.28...4.3)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(VCPKG_MANIFEST_INSTALL OFF)
set(VCPKG_INSTALLED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build/vcpkg_installed)

set (VCPKG_OVERLAY_TRIPLETS ENV{VCPKG_OVERLAY_TRIPLETS})
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(cmake/homebrewClang.cmake)

enable_testing()

include(MonorepoPackages.cmake)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(streamr-native-sdk CXX)

message(STATUS "Root project VCPKG_TARGET_TRIPLET: ${VCPKG_TARGET_TRIPLET}")

foreach(package ${MonorepoPackages})
  add_subdirectory(packages/${package})
endforeach()

