cmake_minimum_required(VERSION 3.28...4.3)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(homebrewClang.cmake)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Module scanning stays OFF globally so that non-module TUs keep clean
# compile commands for clangd; targets with module units or import-using
# sources opt back in via the StreamrModules.cmake helpers.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

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


# C++ modules support (guards, policies, helpers) — must come after
# project() because it inspects the compiler id.
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamrModules.cmake)

# Module façade (MODERNIZATION.md Part 2): the package is now a STATIC
# library whose module interface units re-export the public headers.
# #include consumers are unaffected; import consumers get the BMIs.
streamr_add_module_library(streamr-utils
  FILES
    modules/streamr.utils.cppm
    modules/streamr.utils-all.cppm)
add_library(streamr::streamr-utils ALIAS streamr-utils)

target_include_directories(
  streamr-utils PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>)

find_library(SECP256K1_LIBRARY secp256k1)
# secp256k1 >= 0.7 links the precomputed tables into the main library; the
# separate secp256k1_precomputed archive no longer exists.
find_library(SECP256K1_LIBRARY_PRECOMPUTED secp256k1_precomputed)
if(NOT SECP256K1_LIBRARY_PRECOMPUTED)
    set(SECP256K1_LIBRARY_PRECOMPUTED "")
endif()

message(STATUS "SECP256K1_LIBRARY_PRECOMPUTED: ${SECP256K1_LIBRARY_PRECOMPUTED}")
# PUBLIC (was INTERFACE): the module interface units compile against these
# in their global module fragments.
target_link_libraries(streamr-utils
    PUBLIC Folly::folly
    PUBLIC streamr::streamr-eventemitter
    PUBLIC streamr::streamr-logger
    PUBLIC Boost::uuid
    PUBLIC Boost::endian
    PUBLIC Boost::algorithm
    PUBLIC cryptopp::cryptopp
    PUBLIC ${SECP256K1_LIBRARY}
    PUBLIC ${SECP256K1_LIBRARY_PRECOMPUTED}
)

export(TARGETS streamr-utils
    NAMESPACE streamr::
    FILE streamr-utils-config-in.cmake
    CXX_MODULES_DIRECTORY streamr-utils-modules)
  
file(WRITE "${CMAKE_BINARY_DIR}/streamr-utils-config.cmake" 
    "list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH})\n"
    "find_package(Boost CONFIG REQUIRED)\n"
    "find_package(folly CONFIG REQUIRED)\n"
    "find_package(cryptopp CONFIG REQUIRED)\n"
    "find_library(SECP256K1_LIBRARY secp256k1)\n"
    "find_library(SECP256K1_LIBRARY_PRECOMPUTED secp256k1_precomputed)\n"
    "if(NOT SECP256K1_LIBRARY_PRECOMPUTED)\n"
    "    set(SECP256K1_LIBRARY_PRECOMPUTED \"\")\n"
    "endif()\n"
    "if(NOT TARGET Boost::uuid)\n"
    "    add_library(Boost::uuid INTERFACE IMPORTED)\n"
    "    set_target_properties(Boost::uuid PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})\n"
    "endif()\n"
    "if(NOT TARGET Boost::endian)\n"
    "    add_library(Boost::endian INTERFACE IMPORTED)\n"
    "    set_target_properties(Boost::endian PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})\n"
    "endif()\n"
    "include(${CMAKE_BINARY_DIR}/streamr-utils-config-in.cmake)\n")

# Test/example targets import the modules — skip them where modules are
# unsupported (Android; they are never executed there anyway).
if(NOT IOS AND STREAMR_MODULES_SUPPORTED)
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

  # The tests import streamr.utils (+ streamr.eventemitter), so their
  # sources need module dependency scanning, and imported modules must be
  # linked directly (BMI usability does not ride transitive linkage).
  streamr_enable_imports(streamr-utils-test-unit)
  target_link_libraries(streamr-utils-test-unit
    PUBLIC streamr-utils
    PUBLIC streamr::streamr-eventemitter
    PUBLIC GTest::gtest
    PUBLIC streamr-utils-test-main
  )
  
  if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
    include(GoogleTest)
    gtest_discover_tests(streamr-utils-test-unit)
  endif()
  
endif()
