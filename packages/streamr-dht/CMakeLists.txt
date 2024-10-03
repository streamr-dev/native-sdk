cmake_minimum_required(VERSION 3.22)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(homebrewClang.cmake)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# makes the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)
set (VCPKG_OVERLAY_TRIPLETS ENV{VCPKG_OVERLAY_TRIPLETS})

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(dht CXX)

find_package(streamr-utils CONFIG REQUIRED)
find_package(streamr-logger CONFIG REQUIRED)
find_package(streamr-proto-rpc CONFIG REQUIRED)
find_package(streamr-eventemitter CONFIG REQUIRED)
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

add_library(streamr-dht)
set_property(TARGET streamr-dht PROPERTY CXX_STANDARD 26)
add_library(streamr::streamr-dht ALIAS streamr-dht)
target_include_directories(streamr-dht 
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PUBLIC $<INSTALL_INTERFACE:include>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
    )

target_sources(streamr-dht 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/packages/dht/protos/DhtRpc.pb.cc
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/packages/proto-rpc/protos/ProtoRpc.pb.cc
    )

target_link_libraries(streamr-dht 
    INTERFACE streamr::streamr-logger
    INTERFACE streamr::streamr-eventemitter
    PUBLIC streamr::streamr-proto-rpc
    INTERFACE streamr::streamr-utils
    PRIVATE protobuf::libprotobuf
    PRIVATE protobuf::libprotoc
    INTERFACE Boost::uuid
    INTERFACE Boost::algorithm
    PRIVATE magic_enum::magic_enum
    PRIVATE Folly::folly
    PRIVATE OpenSSL::Crypto
    PRIVATE LibDataChannel::LibDataChannelStatic
    INTERFACE ipaddress::ipaddress
    )

export(TARGETS streamr-dht 
    NAMESPACE streamr::
    FILE streamr-dht-config-in.cmake)
  
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

if(NOT IOS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_library(streamr-dht-test-main
    src/CustomGtestMain.cpp
    )

  target_link_libraries(streamr-dht-test-main
    PUBLIC Folly::folly
        PUBLIC GTest::gtest
    )

    add_executable(streamr-dht-test-unit
        test/unit/WebsocketClientConnectionTest.cpp
        test/unit/WebsocketClientConnectorRpcLocalTest.cpp
        test/unit/CertificateHelperTest.cpp
        test/unit/ConnectionManagerTest.cpp
        test/unit/WebsocketClientConnectorRpcRemoteTest.cpp
        test/unit/IdentifiersTest.cpp
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
        test/unit/RoutingRpcCommunicatorTest.cpp
    )

    target_include_directories(streamr-dht-test-unit
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test/utils>
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
        )

    target_link_libraries(streamr-dht-test-unit 
        PUBLIC streamr-dht
        INTERFACE streamr::streamr-logger
        PUBLIC GTest::gtest
        PUBLIC streamr-dht-test-main
        PUBLIC Folly::folly
    )

    add_executable(streamr-dht-test-integration
        test/integration/WebsocketClientServerTest.cpp
        test/integration/ConnectionLockingTest.cpp
    )

    target_include_directories(streamr-dht-test-integration
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test/utils>
    )

    target_link_libraries(streamr-dht-test-integration 
        PUBLIC streamr-dht
        INTERFACE streamr::streamr-logger
        PUBLIC GTest::gtest
        PUBLIC streamr-dht-test-main
    )
    if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
        include(GoogleTest)
        gtest_discover_tests(streamr-dht-test-unit)
        gtest_discover_tests(streamr-dht-test-integration)
    endif()
endif()
