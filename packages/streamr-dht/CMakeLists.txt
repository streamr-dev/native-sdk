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

project(dht CXX)

# add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
# add_link_options(-fsanitize=address -fno-omit-frame-pointer)

# Opt-in ThreadSanitizer build for the connection-layer concurrency work
# (phase A0). Enable with -DSTREAMR_DHT_TSAN=ON and run the
# --gtest_repeat stress loops under it. NOTE: the vcpkg dependencies
# (folly, protobuf, ...) are NOT built with TSAN, so their internals show
# up as uninstrumented and some reports need a suppressions file; this is
# a developer aid, not yet a clean CI leg (tracked in
# trackerless-network-completion-plan.md phase A0).
option(STREAMR_DHT_TSAN "Build streamr-dht with ThreadSanitizer" OFF)
if(STREAMR_DHT_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=thread)
endif()

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
find_package(Protobuf REQUIRED)
find_package(Boost CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(folly REQUIRED)
find_package(LibDataChannel CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ipaddress CONFIG REQUIRED)

if(NOT TARGET Boost::uuid)
  add_library(Boost::uuid INTERFACE IMPORTED)
  set_target_properties(Boost::uuid PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
endif()

# C++ modules support (guards, policies, helpers) — must come after
# project() because it inspects the compiler id.
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamrModules.cmake)

add_library(streamr-dht)
set_property(TARGET streamr-dht PROPERTY CXX_STANDARD 26)
add_library(streamr::streamr-dht ALIAS streamr-dht)

# CONSOLIDATED (MODERNIZATION.md Phase 2.6): one partition per former
# public header, in the same directory tree the headers had (the
# partition files are the source of truth), plus :protos over the
# generated DhtRpc trio and the primary interface unit that re-exports
# every partition.
file(GLOB_RECURSE STREAMR_DHT_MODULE_UNITS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/*.cppm)
streamr_target_module_sources(streamr-dht FILES ${STREAMR_DHT_MODULE_UNITS})
# Only the generated protobuf code remains a header consumers may need
# textually.
target_include_directories(streamr-dht
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
    )

target_sources(streamr-dht 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/packages/dht/protos/DhtRpc.pb.cc
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/packages/proto-rpc/protos/ProtoRpc.pb.cc
    )

# PUBLIC (was INTERFACE/PRIVATE): the module interface units compile the
# public headers in their global module fragments, and consumer build
# trees recompile those units from the export — every dependency the
# headers reach must be an exported usage requirement.
target_link_libraries(streamr-dht
    PUBLIC streamr::streamr-logger
    PUBLIC streamr::streamr-eventemitter
    PUBLIC streamr::streamr-proto-rpc
    PUBLIC streamr::streamr-utils
    PUBLIC protobuf::libprotobuf
    PUBLIC Boost::uuid
    PUBLIC Boost::algorithm
    PUBLIC magic_enum::magic_enum
    PUBLIC Folly::folly
    PUBLIC OpenSSL::Crypto
    PUBLIC LibDataChannel::LibDataChannel
    PUBLIC ipaddress::ipaddress
    )

export(TARGETS streamr-dht
    NAMESPACE streamr::
    FILE streamr-dht-config-in.cmake
    CXX_MODULES_DIRECTORY streamr-dht-modules)
  
file(WRITE "${CMAKE_BINARY_DIR}/streamr-dht-config.cmake" 
    "list(APPEND CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})\n"
    "find_package(streamr-logger CONFIG REQUIRED)\n"
    "find_package(streamr-utils CONFIG REQUIRED)\n"
    "find_package(streamr-proto-rpc CONFIG REQUIRED)\n"
    "find_package(streamr-eventemitter CONFIG REQUIRED)\n"
    "find_package(Protobuf REQUIRED)\n"
    "find_package(Boost CONFIG REQUIRED)\n"
    "find_package(magic_enum CONFIG REQUIRED)\n"
    "find_package(folly REQUIRED)\n"
    "find_package(LibDataChannel CONFIG REQUIRED)\n"
    "find_package(OpenSSL REQUIRED)\n"
    "find_package(ipaddress CONFIG REQUIRED)\n"
    "if(NOT TARGET Boost::uuid)\n"
    "  add_library(Boost::uuid INTERFACE IMPORTED)\n"
    "  set_target_properties(Boost::uuid PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})\n"
    "endif()\n"
    "include(${CMAKE_BINARY_DIR}/streamr-dht-config-in.cmake)\n")

# Test/example targets import the modules — skip them where modules are
# unsupported (Android; they are never executed there anyway).
if(NOT IOS AND STREAMR_MODULES_SUPPORTED)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_library(streamr-dht-test-main
    src/CustomGtestMain.cpp
    )

  target_link_libraries(streamr-dht-test-main
    PUBLIC Folly::folly
        PUBLIC GTest::gtest
    )

    # Test-only helpers (mock factories, gtest assertion formatters)
    # ported from the TS package's test/utils; a module library of its
    # own so both test targets can import streamr.dht.TestUtils.
    streamr_add_module_library(streamr-dht-test-utils
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/test/utils
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/test/utils/TestUtils.cppm)
    target_include_directories(streamr-dht-test-utils
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>)
    target_link_libraries(streamr-dht-test-utils
        PUBLIC streamr-dht
        PUBLIC streamr::streamr-utils
        PUBLIC GTest::gtest
    )

    add_executable(streamr-dht-test-unit
        test/unit/WebsocketClientConnectionTest.cpp
        test/unit/WebsocketClientConnectorRpcLocalTest.cpp
        test/unit/CertificateHelperTest.cpp
        test/unit/ConnectionManagerTest.cpp
        test/unit/WebsocketClientConnectorRpcRemoteTest.cpp
        test/unit/IdentifiersTest.cpp
        test/unit/CreatePeerDescriptorTest.cpp
        test/unit/WebrtcConnectionTest.cpp
        test/unit/WebrtcConnectorTest.cpp
        test/unit/ConnectivityRequestHandlerTest.cpp
        test/unit/ConnectionLockRpcRemoteTest.cpp
        test/unit/ConnectorFacadeTest.cpp
        test/unit/ConnectionsViewTest.cpp
        test/unit/WebsocketServerConnectorTest.cpp
        test/unit/OffererTest.cpp
        test/unit/DhtCallContextTest.cpp
        test/unit/PendingConnectionTest.cpp
        test/unit/ConnectionLockRpcLocalTest.cpp
        test/unit/EndpointTest.cpp
        test/unit/ConnectingEndpointStateTest.cpp
        test/unit/ConnectedEndpointStateTest.cpp
        test/unit/DisconnectedEndpointStateTest.cpp
        test/unit/InitialEndpointStateTest.cpp
        test/unit/FakeTransportTest.cpp
        test/unit/EndpointStateTest.cpp
        test/unit/OutgoingHandshakerTest.cpp
        test/unit/IncomingHandshakerTest.cpp
        test/unit/EndpointStateInterfaceTest.cpp
        test/unit/WebsocketConnectionTest.cpp
        test/unit/TransportTest.cpp
        test/unit/HandshakerTest.cpp
        test/unit/RoutingRpcCommunicatorTest.cpp
        test/unit/ConnectionLockerTest.cpp
        test/unit/AddressToolsTest.cpp
        test/unit/DuplicateDetectorTest.cpp
        test/unit/ConnectionLockStatesTest.cpp
        test/unit/ConnectivityTest.cpp
        test/unit/IPendingConnectionTest.cpp
        test/unit/VersionTest.cpp
        test/unit/WebsocketClientConnectorTest.cpp
        test/unit/WebsocketServerConnectionTest.cpp
        test/unit/WebsocketServerTest.cpp
        test/unit/ListeningRpcCommunicatorTest.cpp
        test/unit/CustomMatchersTest.cpp
        test/unit/SimulatorTest.cpp
        test/unit/SortedContactListTest.cpp
        test/unit/RandomContactListTest.cpp
        test/unit/getClosestNodesTest.cpp
        test/unit/RingContactListTest.cpp
        test/unit/KBucketTest.cpp
        test/unit/DhtNodeRpcLocalTest.cpp
        test/unit/PeerManagerTest.cpp
        test/unit/RoutingSessionTest.cpp
        test/unit/RouterTest.cpp
        test/unit/RecursiveOperationSessionTest.cpp
        test/unit/RecursiveOperationManagerTest.cpp
        test/unit/LocalDataStoreTest.cpp
        test/unit/StoreRpcLocalTest.cpp
        test/unit/StoreManagerTest.cpp
        test/unit/DiscoverySessionTest.cpp
    )

    target_include_directories(streamr-dht-test-unit
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test/utils>
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
        )

    # The tests import streamr.dht (+ sibling modules), so their sources
    # need module dependency scanning, and imported modules must be linked
    # directly (BMI usability does not ride transitive linkage).
    streamr_enable_imports(streamr-dht-test-unit)
    target_link_libraries(streamr-dht-test-unit
        PUBLIC streamr-dht
        PUBLIC streamr-dht-test-utils
        PUBLIC streamr::streamr-logger
        PUBLIC streamr::streamr-eventemitter
        PUBLIC streamr::streamr-utils
        PUBLIC streamr::streamr-proto-rpc
        PUBLIC GTest::gtest
        PUBLIC streamr-dht-test-main
        PUBLIC Folly::folly
        PUBLIC GTest::gmock
        PUBLIC GTest::gmock_main
    )

    add_executable(streamr-dht-test-integration
        test/integration/WebsocketClientServerTest.cpp
        test/integration/ConnectionLockingTest.cpp
        test/integration/SimultaneousConnectionsTest.cpp
        test/integration/ConnectionManagerIntegrationTest.cpp
        test/integration/DhtNodeRpcRemoteTest.cpp
        test/integration/RouterRpcRemoteTest.cpp
        test/integration/MultipleEntryPointJoiningTest.cpp
        test/integration/DhtNodeTest.cpp
        test/integration/MockLayer1Layer0Test.cpp
        test/integration/Layer1ScaleTest.cpp
        test/integration/DhtNodeExternalApiTest.cpp
        test/integration/KademliaCorrectnessTest.cpp
        test/integration/ConnectivityCheckingTest.cpp
        test/integration/WebsocketConnectionManagementTest.cpp
        test/integration/WebrtcConnectorRpcTest.cpp
        test/integration/WebrtcConnectionManagementTest.cpp
        test/integration/RpcConnectionsOverWebrtcTest.cpp
    )

    target_include_directories(streamr-dht-test-integration
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test/utils>
    )

    streamr_enable_imports(streamr-dht-test-integration)
    target_link_libraries(streamr-dht-test-integration
        PUBLIC streamr-dht
        PUBLIC streamr-dht-test-utils
        PUBLIC streamr::streamr-logger
        PUBLIC streamr::streamr-eventemitter
        PUBLIC streamr::streamr-utils
        PUBLIC streamr::streamr-proto-rpc
        PUBLIC GTest::gtest
        PUBLIC streamr-dht-test-main
    )
    if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
        include(GoogleTest)
        gtest_discover_tests(streamr-dht-test-unit)
        gtest_discover_tests(streamr-dht-test-integration)
    endif()
endif()
