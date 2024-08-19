cmake_minimum_required(VERSION 3.22)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(homebrewClang.cmake)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# makes the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)
set (VCPKG_OVERLAY_TRIPLETS ENV{VCPKG_OVERLAY_TRIPLETS})

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(proto-rpc CXX)

find_package(streamr-logger CONFIG REQUIRED)
find_package(streamr-eventemitter CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(folly REQUIRED)

if(NOT TARGET Boost::uuid)
  add_library(Boost::uuid INTERFACE IMPORTED)
  set_target_properties(Boost::uuid PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
endif()

add_library(streamr-proto-rpc)
set_property(TARGET streamr-proto-rpc PROPERTY CXX_STANDARD 26)
add_library(streamr::streamr-proto-rpc ALIAS streamr-proto-rpc)
target_include_directories(streamr-proto-rpc 
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PUBLIC $<INSTALL_INTERFACE:include>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
    )

target_sources(streamr-proto-rpc 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/ProtoRpc.pb.cc)

target_link_libraries(streamr-proto-rpc 
    INTERFACE streamr::streamr-logger
    INTERFACE streamr::streamr-eventemitter
    PRIVATE protobuf::libprotobuf
    PRIVATE protobuf::libprotoc
    INTERFACE Boost::uuid
    PRIVATE magic_enum::magic_enum
    PUBLIC Folly::folly
    )

add_executable(protobuf-streamr-plugin src/PluginCodeGeneratorMain.cpp include/streamr-proto-rpc/PluginCodeGenerator.hpp)

target_include_directories(
    protobuf-streamr-plugin PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

target_link_libraries(protobuf-streamr-plugin 
    PRIVATE protobuf::libprotobuf protobuf::libprotoc)

enable_testing()
find_package(GTest CONFIG REQUIRED)

add_library(streamr-proto-rpc-test-main
    src/CustomGtestMain.cpp
)

target_link_libraries(streamr-proto-rpc-test-main
    PUBLIC Folly::folly
    PUBLIC GTest::gtest
)

add_executable(streamr-proto-rpc-test-unit
    test/proto/HelloRpc.pb.cc
    test/proto/TestProtos.pb.cc 
    test/proto/WakeUpRpc.pb.cc
    test/unit/ServerRegistryTest.cpp
    test/unit/RpcCommunicatorTest.cpp
)

target_include_directories(streamr-proto-rpc-test-unit 
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test/proto>
)

target_link_libraries(streamr-proto-rpc-test-unit 
    PUBLIC streamr-proto-rpc
    PUBLIC GTest::gtest 
    PUBLIC streamr-proto-rpc-test-main
    PUBLIC GTest::gmock 
    PUBLIC Folly::folly
    )
    
include(GoogleTest)
gtest_discover_tests(streamr-proto-rpc-test-unit)

add_executable(streamr-proto-rpc-test-integration
    test/proto/HelloRpc.pb.cc
    test/proto/HelloRpc.client.pb.h
    test/proto/TestProtos.pb.cc 
    test/proto/WakeUpRpc.pb.cc
    test/integration/ProtoRpcTest.cpp
)

target_include_directories(streamr-proto-rpc-test-integration 
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test/proto>
)

target_link_libraries(streamr-proto-rpc-test-integration 
    PUBLIC streamr-proto-rpc
    PUBLIC GTest::gtest 
    PUBLIC streamr-proto-rpc-test-main 
    PUBLIC GTest::gmock 
    PUBLIC Folly::folly
    )
    
include(GoogleTest)
gtest_discover_tests(streamr-proto-rpc-test-integration)

add_executable(streamr-proto-rpc-example-hello
    examples/hello/hello.cpp
    examples/hello/proto/HelloRpc.pb.cc
)

target_link_libraries(streamr-proto-rpc-example-hello 
    PUBLIC streamr-proto-rpc
    PUBLIC Folly::folly
)

target_include_directories(streamr-proto-rpc-example-hello 
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/examples/hello/proto>
)

add_executable(streamr-proto-rpc-example-routed-hello
    examples/routed-hello/routedhello.cpp
    examples/routed-hello/proto/RoutedHelloRpc.pb.cc
)

target_link_libraries(streamr-proto-rpc-example-routed-hello
    PUBLIC streamr-proto-rpc
    PUBLIC Folly::folly
)

target_include_directories(streamr-proto-rpc-example-routed-hello
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/examples/routed-hello/proto>
)
