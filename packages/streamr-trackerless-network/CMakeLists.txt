cmake_minimum_required(VERSION 3.22)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(${CMAKE_CURRENT_LIST_DIR}/homebrewClang.cmake)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# makes the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)
set (VCPKG_OVERLAY_TRIPLETS ENV{VCPKG_OVERLAY_TRIPLETS})

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(trackerless-network CXX)

find_package(streamr-utils CONFIG REQUIRED)
find_package(streamr-logger CONFIG REQUIRED)
find_package(streamr-proto-rpc CONFIG REQUIRED)
find_package(streamr-eventemitter CONFIG REQUIRED)
find_package(streamr-dht CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(folly REQUIRED)

if(NOT TARGET Boost::uuid)
  add_library(Boost::uuid INTERFACE IMPORTED)
  set_target_properties(Boost::uuid PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
endif()

add_library(streamr-trackerless-network)
set_property(TARGET streamr-trackerless-network PROPERTY CXX_STANDARD 26)
add_library(streamr::streamr-trackerless-network ALIAS streamr-trackerless-network)
target_include_directories(streamr-trackerless-network 
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PUBLIC $<INSTALL_INTERFACE:include>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
    )

target_sources(streamr-trackerless-network 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/packages/network/protos/NetworkRpc.pb.cc
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/packages/proto-rpc/protos/ProtoRpc.pb.cc
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/packages/dht/protos/DhtRpc.pb.cc
    )

target_compile_options(streamr-trackerless-network PUBLIC -fPIC)
    
target_link_libraries(streamr-trackerless-network 
    INTERFACE streamr::streamr-logger
    INTERFACE streamr::streamr-eventemitter
    PUBLIC streamr::streamr-proto-rpc
    PUBLIC streamr::streamr-dht
    INTERFACE streamr::streamr-utils
    INTERFACE Boost::algorithm
    PRIVATE protobuf::libprotobuf
    PRIVATE protobuf::libprotoc
    INTERFACE Boost::uuid
    PRIVATE magic_enum::magic_enum
    PRIVATE Folly::folly
    )

export(TARGETS streamr-trackerless-network 
    NAMESPACE streamr::
    FILE streamr-trackerless-network-config-in.cmake)
  
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

if(NOT IOS)
     enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_library(streamr-trackerless-network-test-main
    src/CustomGtestMain.cpp
    )

    target_link_libraries(streamr-trackerless-network-test-main
        PUBLIC Folly::folly
        PUBLIC GTest::gtest
    )

    add_executable(streamr-trackerless-network-test-unit
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

    target_link_libraries(streamr-trackerless-network-test-unit 
        PUBLIC streamr-trackerless-network
        INTERFACE streamr::streamr-logger
        PUBLIC GTest::gtest
        streamr-trackerless-network-test-main
        PUBLIC Folly::folly
    )

     add_executable(streamr-trackerless-network-test-integration
         test/integration/ProxyClientTsIntegrationTest.cpp
     )

     target_link_libraries(streamr-trackerless-network-test-integration 
         PUBLIC streamr-trackerless-network
         INTERFACE streamr::streamr-logger
         PUBLIC GTest::gtest
         PUBLIC streamr-trackerless-network-test-main
     )
     
     if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
         #include(GoogleTest)
         # gtest_discover_tests(streamr-trackerless-network-test-unit)
         #gtest_discover_tests(streamr-trackerless-network-test-integration)
     
         add_test(
            NAME ts-integration-test
            COMMAND "${CMAKE_CURRENT_LIST_DIR}/run-ts-integration-tests.sh"
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
            )

    endif()
endif()
