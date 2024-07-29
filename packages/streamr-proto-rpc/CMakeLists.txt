cmake_minimum_required(VERSION 3.22)
include(homebrewClang.cmake)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# makes the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(proto-rpc CXX)

find_package(streamr-logger CONFIG REQUIRED)
find_package(protobuf REQUIRED)
#find_package(streamr-utils CONFIG REQUIRED)

add_executable(proto-rpc src/main.cpp)
target_link_libraries(proto-rpc 
    PRIVATE streamr::streamr-logger)

add_executable(protobuf-streamr-plugin src/PluginCodeGeneratorMain.cpp include/streamr-proto-rpc/PluginCodeGenerator.hpp)

target_include_directories(
    protobuf-streamr-plugin PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

target_link_libraries(protobuf-streamr-plugin PRIVATE protobuf::libprotobuf protobuf::libprotoc)
