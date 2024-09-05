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
find_package(libdatachannel CONFIG REQUIRED)
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

if(NOT IOS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_executable(streamr-dht-test-unit
        test/unit/WebsocketClientConnectionTest.cpp
        test/unit/WebsocketClientConnectorRpcLocalTest.cpp
        test/unit/WebsocketClientConnectorRpcRemoteTest.cpp
        test/unit/WebsocketServerConnectionTest.cpp
        test/unit/WebsocketServerConnectorTest.cpp
        test/unit/CertificateHelperTest.cpp
        test/unit/RoutingRpcCommunicatorTest.cpp
        test/unit/ConnectionManagerTest.cpp
    )

    #target_include_directories(streamr-dht-test-unit 
    #    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test/proto>
    #)

    target_link_libraries(streamr-dht-test-unit 
        PUBLIC streamr-dht
        INTERFACE streamr::streamr-logger
        PUBLIC GTest::gtest
        PUBLIC GTest::gtest_main
    )

    add_executable(streamr-dht-test-integration
        test/integration/WebsocketClientServerTest.cpp
    )

    target_link_libraries(streamr-dht-test-integration 
        PUBLIC streamr-dht
        INTERFACE streamr::streamr-logger
        PUBLIC GTest::gtest
        PUBLIC GTest::gtest_main
    )
    if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
        include(GoogleTest)
        gtest_discover_tests(streamr-dht-test-unit)
        gtest_discover_tests(streamr-dht-test-integration)
    endif()
endif()
