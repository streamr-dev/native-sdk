cmake_minimum_required(VERSION 3.28...4.3)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(homebrewClang.cmake)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Module scanning stays OFF globally so that non-module TUs keep clean
# compile commands for clangd; targets with module units or import-using
# sources opt back in via the StreamrModules.cmake helpers.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# makes the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)
set (VCPKG_OVERLAY_TRIPLETS ENV{VCPKG_OVERLAY_TRIPLETS})

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(proto-rpc CXX)
find_package(streamr-logger CONFIG REQUIRED)
find_package(streamr-eventemitter CONFIG REQUIRED)
find_package(streamr-utils CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(folly REQUIRED)

if(NOT TARGET Boost::uuid)
  add_library(Boost::uuid INTERFACE IMPORTED)
  set_target_properties(Boost::uuid PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
endif()

# C++ modules support (guards, policies, helpers) — must come after
# project() because it inspects the compiler id.
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamrModules.cmake)

add_library(streamr-proto-rpc)
set_property(TARGET streamr-proto-rpc PROPERTY CXX_STANDARD 26)
add_library(streamr::streamr-proto-rpc ALIAS streamr-proto-rpc)

# Module façade (MODERNIZATION.md Part 2), added to the existing library
# (it also compiles the generated ProtoRpc.pb.cc). :protos wraps the
# generated header; the other partitions mirror the public headers.
streamr_target_module_sources(streamr-proto-rpc
  FILES
    modules/streamr.protorpc.cppm
    modules/streamr.protorpc-protos.cppm
    modules/streamr.protorpc-all.cppm)
target_include_directories(streamr-proto-rpc 
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PUBLIC $<INSTALL_INTERFACE:include>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
    PUBLIC $<INSTALL_INTERFACE:include>
    )

target_sources(streamr-proto-rpc 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/packages/proto-rpc/protos/ProtoRpc.pb.cc)

# PUBLIC (was INTERFACE/PRIVATE): the module interface units compile the
# public headers in their global module fragments, and consumer build
# trees recompile those units from the export — every dependency the
# headers reach must be an exported usage requirement.
target_link_libraries(streamr-proto-rpc
    PUBLIC streamr::streamr-logger
    PUBLIC streamr::streamr-eventemitter
    PUBLIC streamr::streamr-utils
    PUBLIC protobuf::libprotobuf
    PUBLIC Boost::uuid
    PUBLIC magic_enum::magic_enum
    PUBLIC Folly::folly
    )

export(TARGETS streamr-proto-rpc
    NAMESPACE streamr::
    FILE streamr-proto-rpc-config-in.cmake
    CXX_MODULES_DIRECTORY streamr-proto-rpc-modules)
  
file(WRITE "${CMAKE_BINARY_DIR}/streamr-proto-rpc-config.cmake" 
    "list(APPEND CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})\n"
    "find_package(streamr-logger CONFIG REQUIRED)\n"
    "find_package(streamr-eventemitter CONFIG REQUIRED)\n"
    "find_package(Protobuf REQUIRED)\n"
    "find_package(Boost CONFIG REQUIRED)\n"
    "find_package(magic_enum CONFIG REQUIRED)\n"
    "find_package(folly REQUIRED)\n"
    "if(NOT TARGET Boost::uuid)\n"
    "  add_library(Boost::uuid INTERFACE IMPORTED)\n"
    "  set_target_properties(Boost::uuid PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})\n"
    "endif()\n"
    "include(${CMAKE_BINARY_DIR}/streamr-proto-rpc-config-in.cmake)\n")

# Test/example targets import the modules — skip them where modules are
# unsupported (Android; they are never executed there anyway).
if(NOT IOS AND STREAMR_MODULES_SUPPORTED)
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

    streamr_enable_imports(streamr-proto-rpc-test-unit)
    target_include_directories(streamr-proto-rpc-test-unit
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test/proto>
    )

    target_link_libraries(streamr-proto-rpc-test-unit
        PUBLIC streamr-proto-rpc
        # Imported modules are usable only by DIRECT consumers: a target
        # whose sources `import streamr.logger;` must link the imported
        # target itself (transitive linkage does not propagate BMIs).
        PUBLIC streamr::streamr-logger
        PUBLIC GTest::gtest
        PUBLIC streamr-proto-rpc-test-main
        PUBLIC GTest::gmock
        PUBLIC Folly::folly
    )
    if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
      include(GoogleTest)
      gtest_discover_tests(streamr-proto-rpc-test-unit)
    endif()

    add_executable(streamr-proto-rpc-test-integration
        test/proto/HelloRpc.pb.cc
        test/proto/HelloRpc.client.pb.h
        test/proto/TestProtos.pb.cc 
        test/proto/WakeUpRpc.pb.cc
        test/integration/ProtoRpcTest.cpp
    )

    streamr_enable_imports(streamr-proto-rpc-test-integration)
    target_include_directories(streamr-proto-rpc-test-integration
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test/proto>
    )

    target_link_libraries(streamr-proto-rpc-test-integration
        PUBLIC streamr-proto-rpc
        # Direct links for directly-imported modules (see the unit test).
        PUBLIC streamr::streamr-logger
        PUBLIC streamr::streamr-eventemitter
        PUBLIC GTest::gtest
        PUBLIC streamr-proto-rpc-test-main
        PUBLIC GTest::gmock
        PUBLIC Folly::folly
    )
    if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
      include(GoogleTest)
      gtest_discover_tests(streamr-proto-rpc-test-integration)
    endif()
    
    add_executable(streamr-proto-rpc-example-hello
        examples/hello/hello.cpp
        examples/hello/proto/HelloRpc.pb.cc
    )

    target_link_libraries(streamr-proto-rpc-example-hello 
        PUBLIC streamr-proto-rpc
        PUBLIC Folly::folly
    )

    streamr_enable_imports(streamr-proto-rpc-example-hello)
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

    streamr_enable_imports(streamr-proto-rpc-example-routed-hello)
    target_include_directories(streamr-proto-rpc-example-routed-hello
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/examples/routed-hello/proto>
    )
endif()
