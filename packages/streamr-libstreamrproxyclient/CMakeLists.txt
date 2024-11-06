cmake_minimum_required(VERSION 3.22)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(homebrewClang.cmake)
set(CMAKE_CXX_STANDARD 26)

set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES Off)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# make the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

set (SHAREDLIB_VERSION 1.0.0)
set (SHAREDLIB_SOVERSION 1)

project(streamr-streamrproxyclient CXX)
add_library(streamrproxyclient SHARED
            src/streamrproxyclient.cpp
            include/streamrproxyclient.h include/StreamrProxyClient.hpp
)

set_target_properties(streamrproxyclient PROPERTIES VERSION ${SHAREDLIB_VERSION} SOVERSION ${SHAREDLIB_SOVERSION})
find_package(streamr-trackerless-network CONFIG REQUIRED)
find_package(streamr-dht CONFIG REQUIRED)
find_package(streamr-logger CONFIG REQUIRED)
find_package(streamr-utils CONFIG REQUIRED)
find_package(ada CONFIG REQUIRED)

target_include_directories(streamrproxyclient PUBLIC include)
target_link_libraries(streamrproxyclient 
    PRIVATE streamr::streamr-trackerless-network
    PRIVATE streamr::streamr-logger
    PRIVATE streamr::streamr-dht
    PRIVATE streamr::streamr-utils
    PRIVATE ada::ada
    PRIVATE Folly::folly
)

if (IOS)
    message(STATUS "Building dylib for iOS")
    target_link_libraries(streamrproxyclient ${FOUNDATION_LIBRARY})
    target_compile_definitions(streamrproxyclient PUBLIC IS_BUILDING_SHARED)
    set_target_properties(streamrproxyclient PROPERTIES
        FRAMEWORK TRUE
        FRAMEWORK_VERSION A
        MACOSX_FRAMEWORK_IDENTIFIER network.streamr.streamrproxyclient
        VERSION ${SHAREDLIB_VERSION}
        SOVERSION ${SHAREDLIB_SOVERSION}
        PUBLIC_HEADER include/streamrproxyclient.h
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Petri Savolainen"
    )
    install(CODE "execute_process(COMMAND ${CMAKE_CURRENT_LIST_DIR}/createiosframework.sh)")
    # Pack dist/target-triplet using tgz
    install(CODE "
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar cvzf streamrproxyclient-${VCPKG_TARGET_TRIPLET}-${SHAREDLIB_VERSION}.tgz ${VCPKG_TARGET_TRIPLET}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist
        )
    ")
endif()

if (NOT IOS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    find_package(folly CONFIG REQUIRED)
     
    add_executable(streamr-streamrproxyclient-test-integration test/integration/StreamrProxyClientTest.cpp test/integration/StreamrProxyClientCppTest.cpp)
    target_link_directories(streamr-streamrproxyclient-test-integration PUBLIC ${SHAREDLIB_OUTPUT_DIRECTORY})
    
    target_include_directories(streamr-streamrproxyclient-test-integration PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
    
    target_link_libraries(streamr-streamrproxyclient-test-integration 
        PRIVATE streamrproxyclient
        PRIVATE streamr::streamr-logger
        PRIVATE GTest::gtest 
        PRIVATE GTest::gtest_main
    )

    add_executable(streamr-streamrproxyclient-test-end-to-end test/end-to-end/PublishToTsServerTest.cpp)
    target_link_directories(streamr-streamrproxyclient-test-end-to-end PUBLIC ${CMAKE_CURRENT_LIST_DIR}/build)
    
    target_include_directories(streamr-streamrproxyclient-test-end-to-end PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
    
    target_link_libraries(streamr-streamrproxyclient-test-end-to-end 
        PRIVATE streamrproxyclient
        PRIVATE GTest::gtest 
        PRIVATE GTest::gtest_main
    )

    add_executable(streamrproxyclient-cpp-wrapper-test  ${CMAKE_CURRENT_LIST_DIR}/wrappers/cpp/test/StreamrProxyClientTest.cpp)
    target_link_directories(streamrproxyclient-cpp-wrapper-test PUBLIC ${CMAKE_CURRENT_LIST_DIR}/build)
    
    target_include_directories(streamrproxyclient-cpp-wrapper-test PUBLIC ${CMAKE_CURRENT_LIST_DIR}/wrappers/cpp/include)
    
    target_link_libraries(streamrproxyclient-cpp-wrapper-test 
        PRIVATE streamrproxyclient
        PRIVATE GTest::gtest 
        PRIVATE GTest::gtest_main
    )

    if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
        include(GoogleTest)
        gtest_discover_tests(streamr-streamrproxyclient-test-integration)
        gtest_discover_tests(streamrproxyclient-cpp-wrapper-test)
        add_test(
            NAME ts-end-to-end-test
            COMMAND "${CMAKE_CURRENT_LIST_DIR}/run-ts-end-to-end-tests.sh"
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
            )

        add_test(
            NAME ts-multiple-messages-test
            COMMAND "${CMAKE_CURRENT_LIST_DIR}/run-ts-multiple-messages-test.sh"
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
            )

    endif()
    # Install header to dist/target-triplet
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/streamrproxyclient.h
            DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/dist/${VCPKG_TARGET_TRIPLET}/include)

    # Install library to dist/target-triplet
    install(TARGETS streamrproxyclient
            DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/dist/${VCPKG_TARGET_TRIPLET}/lib/${CMAKE_BUILD_TYPE})

    # Pack dist/target-triplet using tgz
    install(CODE "
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar cvzf streamrproxyclient-${VCPKG_TARGET_TRIPLET}-${SHAREDLIB_VERSION}.tgz ${VCPKG_TARGET_TRIPLET}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist
        )
    ")
endif()

