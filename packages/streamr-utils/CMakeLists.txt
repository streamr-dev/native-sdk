cmake_minimum_required(VERSION 3.22)
include(homebrewClang.cmake)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

set(CMAKE_CXX_STANDARD 26)

project(streamr-utils CXX)

find_package(folly CONFIG REQUIRED)

add_library(streamr-utils INTERFACE)
add_library(streamr::streamr-utils ALIAS streamr-utils)

target_include_directories(
  streamr-utils INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

target_link_libraries(streamr-utils
    INTERFACE Folly::folly
)

export(TARGETS streamr-utils
    NAMESPACE streamr::
    FILE streamr-utils-config-in.cmake)
  
file(WRITE "${CMAKE_BINARY_DIR}/streamr-utils-config.cmake" 
    "list(APPEND CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})\n"
    "include(${CMAKE_BINARY_DIR}/streamr-utils-config-in.cmake)\n")

if(NOT IOS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)

  add_executable(streamr-utils-test-unit test/unit/BrandedTest.cpp)
  target_link_libraries(streamr-utils-test-unit 
    PUBLIC streamr-utils
    PUBLIC GTest::gtest 
    PUBLIC GTest::gtest_main 
  )
  if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
    include(GoogleTest)
    gtest_discover_tests(streamr-utils-test-unit)
  endif()
  
endif()