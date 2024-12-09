cmake_minimum_required(VERSION 3.22)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(homebrewClang.cmake)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

set(CMAKE_CXX_STANDARD 26)

project(streamr-utils CXX)

# add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
# add_link_options(-fsanitize=address -fno-omit-frame-pointer)

find_package(folly CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED)
find_package(cryptopp CONFIG REQUIRED)
find_package(streamr-eventemitter CONFIG REQUIRED)
find_package(streamr-logger CONFIG REQUIRED)

if(NOT TARGET Boost::uuid)
  add_library(Boost::uuid INTERFACE IMPORTED)
  set_target_properties(Boost::uuid PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
endif()

if(NOT TARGET Boost::endian)
  add_library(Boost::endian INTERFACE IMPORTED)
  set_target_properties(Boost::endian PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
endif()


add_library(streamr-utils INTERFACE)
add_library(streamr::streamr-utils ALIAS streamr-utils)

target_include_directories(
  streamr-utils INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

find_library(SECP256K1_LIBRARY secp256k1)
find_library(SECP256K1_LIBRARY_PRECOMPUTED secp256k1_precomputed)

message(STATUS "SECP256K1_LIBRARY_PRECOMPUTED: ${SECP256K1_LIBRARY_PRECOMPUTED}")
target_link_libraries(streamr-utils
    INTERFACE Folly::folly
    INTERFACE streamr::streamr-eventemitter
    INTERFACE streamr::streamr-logger
    INTERFACE Boost::uuid
    INTERFACE Boost::endian
    INTERFACE Boost::algorithm
    INTERFACE cryptopp::cryptopp
    INTERFACE ${SECP256K1_LIBRARY}
    INTERFACE ${SECP256K1_LIBRARY_PRECOMPUTED}
)

export(TARGETS streamr-utils
    NAMESPACE streamr::
    FILE streamr-utils-config-in.cmake)
  
file(WRITE "${CMAKE_BINARY_DIR}/streamr-utils-config.cmake" 
    "list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH})\n"
    "find_package(Boost CONFIG REQUIRED)\n"
    "find_package(folly CONFIG REQUIRED)\n"
    "find_package(cryptopp CONFIG REQUIRED)\n"
    "find_library(SECP256K1_LIBRARY secp256k1)\n"
    "find_library(SECP256K1_LIBRARY_PRECOMPUTED secp256k1_precomputed)\n"
    "if(NOT TARGET Boost::uuid)\n"
    "    add_library(Boost::uuid INTERFACE IMPORTED)\n"
    "    set_target_properties(Boost::uuid PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})\n"
    "endif()\n"
    "if(NOT TARGET Boost::endian)\n"
    "    add_library(Boost::endian INTERFACE IMPORTED)\n"
    "    set_target_properties(Boost::endian PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})\n"
    "endif()\n"
    "include(${CMAKE_BINARY_DIR}/streamr-utils-config-in.cmake)\n")

if(NOT IOS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)

  add_library(streamr-utils-test-main
    src/CustomGtestMain.cpp
    )

  target_link_libraries(streamr-utils-test-main
    PUBLIC Folly::folly
    PUBLIC GTest::gtest
  )

  add_executable(streamr-utils-test-unit 
    test/unit/BrandedTest.cpp
    test/unit/AbortableTimersTest.cpp
    test/unit/waitForEventTest.cpp
    test/unit/waitForConditionTest.cpp
    test/unit/runAndWaitForEventsTest.cpp
    test/unit/toCoroTaskTest.cpp
    test/unit/collectTest.cpp
    test/unit/toEthereumAddressOrENSNameTest.cpp
    test/unit/StreamPartIDTest.cpp
    test/unit/RetryUtilsTest.cpp
    test/unit/StreamIDTest.cpp
    test/unit/ReplayEventEmitterWrapperTest.cpp
    test/unit/EthereumAddressTest.cpp
    test/unit/ENSNameTest.cpp
    test/unit/AbortControllerTest.cpp
    test/unit/SigningUtilsTest.cpp
    test/unit/BinaryUtilsTest.cpp
  )

  target_link_libraries(streamr-utils-test-unit 
    PUBLIC streamr-utils
    PUBLIC GTest::gtest 
    PUBLIC streamr-utils-test-main 
  )
  
  if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
    include(GoogleTest)
    gtest_discover_tests(streamr-utils-test-unit)
  endif()
  
endif()
