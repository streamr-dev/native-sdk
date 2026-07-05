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

# CONSOLIDATED (MODERNIZATION.md Phase 2.6): one named sub-module per
# former public header (settled architecture: no umbrella; consumers and
# tests import the individual sub-modules), plus streamr.protorpc.protos
# over the generated ProtoRpc types. The library also compiles the
# generated ProtoRpc.pb.cc.
file(GLOB_RECURSE STREAMR_PROTORPC_MODULE_UNITS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/*.cppm)
streamr_target_module_sources(streamr-proto-rpc
  FILES ${STREAMR_PROTORPC_MODULE_UNITS})
# Only the generated protobuf code remains a header consumers may need
# textually.
target_include_directories(streamr-proto-rpc
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
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

# The protoc codegen plugin is host tooling (invoked by proto.sh to
# regenerate the checked-in *.pb.* files) — never built for cross
# targets: vcpkg does not provide protobuf::libprotoc for Android/iOS
# and the plugin runs on the developer machine anyway. (Previously this
# sat inside the modules block below and was skipped on Android only as
# a side effect of the modules gate being OFF there.)
# NOTE the explicit IOS check: iOS package builds keep the host
# CMAKE_SYSTEM_NAME (they use the Homebrew compiler and only swap the
# SDK/sysroot — see homebrewClang.cmake), so CMAKE_CROSSCOMPILING is
# FALSE there; IOS is set by toolchains/ios.toolchain.cmake.
if(NOT IOS AND NOT CMAKE_CROSSCOMPILING)
    add_executable(protobuf-streamr-plugin src/PluginCodeGeneratorMain.cpp src/PluginCodeGenerator.hpp)

    target_include_directories(
        protobuf-streamr-plugin PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

    target_link_libraries(protobuf-streamr-plugin
        PRIVATE protobuf::libprotobuf protobuf::libprotoc)
endif()

# Test/example targets import the modules — skip them where modules are
# unsupported (they are never executed on Android anyway).
if(NOT IOS AND STREAMR_MODULES_SUPPORTED)
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
        test/proto/TestProtos.pb.cc
        test/proto/WakeUpRpc.pb.cc
        test/integration/ProtoRpcTest.cpp
    )

    streamr_enable_imports(streamr-proto-rpc-test-integration)
    # The protoc plugin emits the RPC stubs as module units next to the
    # generated message code; the test imports them directly
    # (streamr.protorpc.test.*). PRIVATE: test-only modules, not part of
    # any export.
    file(GLOB STREAMR_PROTORPC_TEST_STUB_UNITS CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/test/proto/*.cppm)
    target_sources(streamr-proto-rpc-test-integration
        PRIVATE
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/test/proto
        FILES ${STREAMR_PROTORPC_TEST_STUB_UNITS})
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
    file(GLOB STREAMR_PROTORPC_HELLO_STUB_UNITS CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/hello/proto/*.cppm)
    target_sources(streamr-proto-rpc-example-hello
        PRIVATE
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/examples/hello/proto
        FILES ${STREAMR_PROTORPC_HELLO_STUB_UNITS})
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
    file(GLOB STREAMR_PROTORPC_ROUTED_STUB_UNITS CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/routed-hello/proto/*.cppm)
    target_sources(streamr-proto-rpc-example-routed-hello
        PRIVATE
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/examples/routed-hello/proto
        FILES ${STREAMR_PROTORPC_ROUTED_STUB_UNITS})
    target_include_directories(streamr-proto-rpc-example-routed-hello
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/examples/routed-hello/proto>
    )
endif()
