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
set (IOS_SWIFT_PACKAGE_VERSION 1.0.0)
set (ANDROID_LIBRARY_VERSION 1.0.0)

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
    message(STATUS "Building Swift package for iOS")
    install(CODE "
        file(MAKE_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/dist/ios-swift-package\")
        
        execute_process(
            COMMAND tar 
                -czhf streamrproxyclient-ios-swift-package-${IOS_SWIFT_PACKAGE_VERSION}.tgz
                --exclude-from=StreamrProxyClient/.gitignore
                StreamrProxyClient
            WORKING_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/dist/ios-swift-package\"
            RESULT_VARIABLE TAR_RESULT
            OUTPUT_VARIABLE TAR_OUTPUT
            ERROR_VARIABLE TAR_ERROR
        )
        
        message(STATUS \"Working directory: ${CMAKE_CURRENT_SOURCE_DIR}/dist/ios-swift-package\")
        if(TAR_ERROR)
            message(STATUS \"Tar command error: \${TAR_ERROR}\")
        endif()
        
        if(NOT TAR_RESULT EQUAL 0)
            message(FATAL_ERROR \"Failed to create Swift package tar file\")
        endif()
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

    add_executable(streamr-streamrproxyclient-test-end-to-end test/end-to-end/PublishToTsServerTest.cpp test/end-to-end/PublishToTsServerCppTest.cpp)
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

        add_test(
            NAME ts-end-to-end-cpp-test
            COMMAND "${CMAKE_CURRENT_LIST_DIR}/run-ts-end-to-end-cpp-tests.sh"
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
            )

        add_test(
            NAME ts-multiple-messages-cpp-test
            COMMAND "${CMAKE_CURRENT_LIST_DIR}/run-ts-multiple-messages-cpp-test.sh"
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
            )


    endif()
    # Install header to dist/target-triplet
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/streamrproxyclient.h
            DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/dist/${VCPKG_TARGET_TRIPLET}/include)

    # Install library to dist/target-triplet
    install(TARGETS streamrproxyclient
            DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/dist/${VCPKG_TARGET_TRIPLET}/lib/${CMAKE_BUILD_TYPE})

    # Install shared lib to android module and create tgz package
    if(VCPKG_TARGET_TRIPLET MATCHES "android")
        # Install the library
        install(TARGETS streamrproxyclient
                DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/dist/android-library-module/StreamrProxyClient/libs/arm64-v8a)

        message(STATUS "Building Library module for Android")
        
        # Create tar package
        install(CODE "
            # Ensure directory exists
            file(MAKE_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/dist/android-library-module\")

            # Create tar archive
            execute_process(
                COMMAND tar
                    -czhf streamrproxyclient-android-library-module-${ANDROID_LIBRARY_VERSION}.tgz
                    --exclude-from=StreamrProxyClient/.gitignore
                    StreamrProxyClient
                WORKING_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/dist/android-library-module\"
                RESULT_VARIABLE TAR_RESULT
                OUTPUT_VARIABLE TAR_OUTPUT
                ERROR_VARIABLE TAR_ERROR
            )

            # Log working directory
            message(STATUS \"Working directory: ${CMAKE_CURRENT_SOURCE_DIR}/dist/android-library-module\")
            
            # Handle errors
            if(TAR_ERROR)
                message(STATUS \"Tar command error: \${TAR_ERROR}\")
            endif()

            if(NOT TAR_RESULT EQUAL 0)
                message(FATAL_ERROR \"Failed to create Android library module tar file\")
            endif()
        ")
    endif()

    # Pack dist/target-triplet using tgz
    install(CODE "
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar cvzf streamrproxyclient-${VCPKG_TARGET_TRIPLET}-${SHAREDLIB_VERSION}.tgz ${VCPKG_TARGET_TRIPLET}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist
        )
    ")

    # If not ios or android, and build type is Release, copy the .so or .dylib file to the wrappers/go/dist/target-triplet/lib/Release/ directory
    if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android") AND ${CMAKE_BUILD_TYPE} STREQUAL "Release")
        install(CODE "execute_process(COMMAND ${CMAKE_CURRENT_LIST_DIR}/copyfilesforgowrapper.sh ${CMAKE_CURRENT_SOURCE_DIR}/dist/${VCPKG_TARGET_TRIPLET}/lib/${CMAKE_BUILD_TYPE} ${VCPKG_TARGET_TRIPLET} ${CMAKE_CURRENT_LIST_DIR})")
        
        # Also install header to wrappers/go
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/streamrproxyclient.h
                DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/wrappers/go)
    endif()
  
endif()

