cmake_minimum_required(VERSION 3.28...4.3)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(${CMAKE_CURRENT_LIST_DIR}/homebrewClang.cmake)

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

project(trackerless-network CXX)

# Sibling monorepo packages: find_package only when the target is not
# already in this build tree (in the root single-tree build the sibling
# was added via add_subdirectory — the guard makes the root tree
# self-contained, so it configures without the standalone per-package
# build dirs; MODERNIZATION.md "After the consolidation: CI speed").
if(NOT TARGET streamr::streamr-utils)
    find_package(streamr-utils CONFIG REQUIRED)
endif()
if(NOT TARGET streamr::streamr-logger)
    find_package(streamr-logger CONFIG REQUIRED)
endif()
if(NOT TARGET streamr::streamr-proto-rpc)
    find_package(streamr-proto-rpc CONFIG REQUIRED)
endif()
if(NOT TARGET streamr::streamr-eventemitter)
    find_package(streamr-eventemitter CONFIG REQUIRED)
endif()
if(NOT TARGET streamr::streamr-dht)
    find_package(streamr-dht CONFIG REQUIRED)
endif()
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

add_library(streamr-trackerless-network)
set_property(TARGET streamr-trackerless-network PROPERTY CXX_STANDARD 26)
add_library(streamr::streamr-trackerless-network ALIAS streamr-trackerless-network)

# CONSOLIDATED (MODERNIZATION.md Phase 2.6): one partition per former
# public header, in the same directory tree the headers had (the
# partition files are the source of truth), plus :protos over the
# generated NetworkRpc trio and the primary interface unit that
# re-exports every partition.
file(GLOB_RECURSE TRACKERLESS_NETWORK_MODULE_UNITS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/*.cppm)
streamr_target_module_sources(streamr-trackerless-network
  FILES ${TRACKERLESS_NETWORK_MODULE_UNITS})
# CONSOLIDATED: the include/ tree is gone (the code lives in the module
# units); only the generated protobuf code remains a header consumers
# may need textually.
target_include_directories(streamr-trackerless-network
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
    )

target_sources(streamr-trackerless-network 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/packages/network/protos/NetworkRpc.pb.cc
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/packages/proto-rpc/protos/ProtoRpc.pb.cc
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/packages/dht/protos/DhtRpc.pb.cc
    )

target_compile_options(streamr-trackerless-network PUBLIC -fPIC)
    
# PUBLIC (was INTERFACE/PRIVATE): consumer build trees recompile the
# module units from the export — every dependency the headers reach must
# be an exported usage requirement.
target_link_libraries(streamr-trackerless-network
    PUBLIC streamr::streamr-logger
    PUBLIC streamr::streamr-eventemitter
    PUBLIC streamr::streamr-proto-rpc
    PUBLIC streamr::streamr-dht
    PUBLIC streamr::streamr-utils
    PUBLIC Boost::algorithm
    PUBLIC protobuf::libprotobuf
    PUBLIC Boost::uuid
    PUBLIC magic_enum::magic_enum
    PUBLIC Folly::folly
    )

export(TARGETS streamr-trackerless-network
    NAMESPACE streamr::
    FILE streamr-trackerless-network-config-in.cmake
    CXX_MODULES_DIRECTORY streamr-trackerless-network-modules)
  
file(WRITE "${CMAKE_BINARY_DIR}/streamr-trackerless-network-config.cmake" 
    "list(APPEND CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})\n"
    "find_package(streamr-utils CONFIG REQUIRED)\n"
    "find_package(streamr-logger CONFIG REQUIRED)\n"
    "find_package(streamr-proto-rpc CONFIG REQUIRED)\n"
    "find_package(streamr-eventemitter CONFIG REQUIRED)\n"
    "find_package(streamr-dht CONFIG REQUIRED)\n"
    "find_package(Protobuf REQUIRED)\n"
    "find_package(Boost CONFIG REQUIRED)\n"
    "find_package(magic_enum CONFIG REQUIRED)\n"
    "find_package(folly REQUIRED)\n"
    "if(NOT TARGET Boost::uuid)\n"
    "  add_library(Boost::uuid INTERFACE IMPORTED)\n"
    "  set_target_properties(Boost::uuid PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})\n"
    "endif()\n"
    "include(${CMAKE_BINARY_DIR}/streamr-trackerless-network-config-in.cmake)\n")

# Test/example targets import the modules — skip them where modules are
# unsupported (Android; they are never executed there anyway).
if(NOT IOS AND STREAMR_MODULES_SUPPORTED)
     enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_library(streamr-trackerless-network-test-main
    src/CustomGtestMain.cpp
    )

    target_link_libraries(streamr-trackerless-network-test-main
        PUBLIC Folly::folly
        PUBLIC GTest::gtest
    )

    # Test-only helpers (mock factories) ported from the TS package's
    # test/utils; a module library of its own so the test targets can
    # import streamr.trackerlessnetwork.TestUtils.
    streamr_add_module_library(streamr-trackerless-network-test-utils
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/test/utils
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/test/utils/TestUtils.cppm)
    target_include_directories(streamr-trackerless-network-test-utils
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>)
    target_link_libraries(streamr-trackerless-network-test-utils
        PUBLIC streamr-trackerless-network
        PUBLIC streamr::streamr-dht
        PUBLIC streamr::streamr-utils
    )

    add_executable(streamr-trackerless-network-test-unit
        test/unit/TestUtilsTest.cpp
        test/unit/ProxyClientTest.cpp
        test/unit/ProxyConnectionRpcLocalTest.cpp
        test/unit/ProxyConnectionRpcRemoteTest.cpp
        test/unit/PropagationTest.cpp
        test/unit/PropagationTaskStoreTest.cpp
        test/unit/UtilsTest.cpp
        test/unit/NodeListTest.cpp
        test/unit/FifoMapWithTTLTest.cpp
        test/unit/formStreamPartDeliveryServiceIdTest.cpp
        test/unit/ContentDeliveryRpcRemoteTest.cpp
        test/unit/ContentDeliveryRpcLocalTest.cpp
        test/unit/DuplicateMessageDetectorTest.cpp
    )

    target_include_directories(streamr-trackerless-network-test-unit
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test/utils>
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
        )

    # The tests import streamr.trackerlessnetwork, so their sources need
    # module dependency scanning.
    streamr_enable_imports(streamr-trackerless-network-test-unit)
    target_link_libraries(streamr-trackerless-network-test-unit
        PUBLIC streamr-trackerless-network
        PUBLIC streamr-trackerless-network-test-utils
        PUBLIC streamr::streamr-logger
        PUBLIC GTest::gtest
        streamr-trackerless-network-test-main
        PUBLIC Folly::folly
    )

     add_executable(streamr-trackerless-network-test-integration
         test/integration/ProxyClientTsIntegrationTest.cpp
     )
     # The test imports modules, so its source needs module dependency
     # scanning; imported modules are usable only by DIRECT consumers, so
     # every directly-imported module is linked here.
     streamr_enable_imports(streamr-trackerless-network-test-integration)

     target_link_libraries(streamr-trackerless-network-test-integration
         PUBLIC streamr-trackerless-network
         PUBLIC streamr::streamr-dht
         PUBLIC streamr::streamr-logger
         PUBLIC streamr::streamr-utils
         PUBLIC GTest::gtest
         PUBLIC streamr-trackerless-network-test-main
     )
     
     if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
         include(GoogleTest)
         gtest_discover_tests(streamr-trackerless-network-test-unit)
         #gtest_discover_tests(streamr-trackerless-network-test-integration)
     
         # DISABLED: this test builds the whole streamr-dev/network TS
         # monorepo at a 2024 commit (ts-integration/install.sh), which no
         # longer works on current CI runners. The streamr network TS
         # packages have changed a lot since 2024 — the TS interop harness
         # will be rewritten in a separate project (see MODERNIZATION.md,
         # "Interim — ts-integration tests"). Kept as reference for that
         # rewrite.
         # add_test(
         #    NAME ts-integration-test
         #    COMMAND "${CMAKE_CURRENT_LIST_DIR}/run-ts-integration-tests.sh"
         #    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
         #    )

    endif()
endif()
