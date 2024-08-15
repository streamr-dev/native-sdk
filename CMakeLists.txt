cmake_minimum_required(VERSION 3.22)

set(VCPKG_MANIFEST_INSTALL OFF)
set(VCPKG_INSTALLED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build/vcpkg_installed)


set (VCPKG_OVERLAY_TRIPLETS ENV{VCPKG_OVERLAY_TRIPLETS})
set(CMAKE_CXX_STANDARD 26)
include(homebrewClang.cmake)

enable_testing()

include(MonorepoPackages.cmake)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(streamr-native-sdk CXX)

foreach(package ${MonorepoPackages})
  add_subdirectory(packages/${package})
endforeach()
