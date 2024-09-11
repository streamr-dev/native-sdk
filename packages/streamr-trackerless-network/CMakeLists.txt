cmake_minimum_required(VERSION 3.22)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(homebrewClang.cmake)

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
    
target_link_libraries(streamr-trackerless-network 
    INTERFACE streamr::streamr-logger
    INTERFACE streamr::streamr-eventemitter
    PUBLIC streamr::streamr-proto-rpc
    PUBLIC streamr::streamr-dht
    INTERFACE streamr::streamr-utils
    PRIVATE protobuf::libprotobuf
    PRIVATE protobuf::libprotoc
    INTERFACE Boost::uuid
    PRIVATE magic_enum::magic_enum
    PRIVATE Folly::folly
    )

# if(NOT IOS)
#     enable_testing()
#     find_package(GTest CONFIG REQUIRED)

#     add_executable(streamr-trackerless-network-test-unit
#         test/unit/WebsocketClientConnectionTest.cpp
#     )

#     target_include_directories(streamr-dht-test-unit
#         PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test/utils>
#         PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
#         PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
#         )

#     target_link_libraries(streamr-dht-test-unit 
#         PUBLIC streamr-dht
#         INTERFACE streamr::streamr-logger
#         PUBLIC GTest::gtest
#         PUBLIC GTest::gtest_main
#         PUBLIC Folly::folly
#     )

#     add_executable(streamr-dht-test-integration
#         test/integration/WebsocketClientServerTest.cpp
#     )

#     target_link_libraries(streamr-dht-test-integration 
#         PUBLIC streamr-dht
#         INTERFACE streamr::streamr-logger
#         PUBLIC GTest::gtest
#         PUBLIC GTest::gtest_main
#     )
#     if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
#         include(GoogleTest)
#         gtest_discover_tests(streamr-dht-test-unit)
#         gtest_discover_tests(streamr-dht-test-integration)
#     endif()
# endif()
