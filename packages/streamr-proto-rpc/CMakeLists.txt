cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 23)

# makes the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(proto-rpc CXX)

find_package(streamr-logger CONFIG REQUIRED)
find_package(streamr-utils CONFIG REQUIRED)

add_executable(proto-rpc src/main.cpp)

target_link_libraries(proto-rpc PRIVATE streamr-logger PRIVATE streamr-utils)
