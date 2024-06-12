cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES Off)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# make the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(streamr-json CXX)

find_package(Boost CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
      
if(NOT TARGET Boost::pfr)
  add_library(Boost::pfr INTERFACE IMPORTED)
  set_target_properties(Boost::pfr PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
endif()

add_library(streamr-json INTERFACE)
set_property(TARGET streamr-json PROPERTY CXX_STANDARD 23)

add_library(streamr::streamr-json ALIAS streamr-json)

target_include_directories(
  streamr-json INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

target_link_libraries(streamr-json INTERFACE Boost::pfr)
target_link_libraries(streamr-json INTERFACE nlohmann_json::nlohmann_json)

# TODO: Is there a better way to do this?
# - Apparently not: https://gitlab.kitware.com/cmake/cmake/-/issues/20511
# - At least parse the deps from the vcpkg.json file and use them to find_package() in this file and to add the
# find_package() calls to the wrapper

export(TARGETS streamr-json FILE streamr-json-config-in.cmake)

file(WRITE "${CMAKE_BINARY_DIR}/streamr-json-config.cmake" 
  "find_package(Boost REQUIRED)\n
  find_package(nlohmann_json REQUIRED)\n
  if(NOT TARGET Boost::pfr)\n
    add_library(Boost::pfr INTERFACE IMPORTED)\n
    set_target_properties(Boost::pfr PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})\n
  endif()\n
  include(${CMAKE_BINARY_DIR}/streamr-json-config-in.cmake)\n")

enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(streamr-json-test-unit test/unit/toStringTest.cpp test/unit/toJsonTest.cpp)
target_link_libraries(streamr-json-test-unit 
  PUBLIC streamr-json
  PUBLIC GTest::gtest 
  PUBLIC GTest::gtest_main 
  PUBLIC GTest::gmock 
  PUBLIC GTest::gmock_main)

include(GoogleTest)
gtest_discover_tests(streamr-json-test-unit)
