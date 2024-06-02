cmake_minimum_required(VERSION 3.22)

include(MonorepoPackages.cmake)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(streamr-native-sdk CXX)

foreach(package ${MonorepoPackages})
  add_subdirectory(packages/${package})
endforeach()
