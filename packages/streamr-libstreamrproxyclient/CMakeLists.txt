cmake_minimum_required(VERSION 3.28...4.3)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(homebrewClang.cmake)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Module scanning stays OFF globally so that non-module TUs keep clean
# compile commands for clangd; targets with module units or import-using
# sources opt back in via the StreamrModules.cmake helpers.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES Off)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# make the current package a monorepo package
include(${CMAKE_CURRENT_SOURCE_DIR}/monorepoPackage.cmake)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

include(${CMAKE_CURRENT_SOURCE_DIR}/VERSION.cmake)
set (SHAREDLIB_VERSION ${STREAMRPROXYCLIENT_VERSION})
set (SHAREDLIB_SOVERSION ${STREAMRPROXYCLIENT_SOVERSION})

set (IOS_SWIFT_PACKAGE_VERSION ${STREAMRPROXYCLIENT_VERSION})
set (ANDROID_LIBRARY_VERSION ${STREAMRPROXYCLIENT_VERSION})

project(streamr-streamrproxyclient CXX)

# The library target links Folly::folly directly, so find it HERE,
# unconditionally: it used to arrive only as a side effect of the
# sibling packages' export configs (which run find_package(folly)), and
# on iOS the test block below — which also finds folly — is skipped.
# With the sibling find_package calls guarded out in the root
# single-tree build, this file must find its own direct dependencies.
find_package(folly CONFIG REQUIRED)

# C++ modules support (guards, policies, helpers) — must come after
# project() because it inspects the compiler id.
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamrModules.cmake)

add_library(streamrproxyclient SHARED
            src/streamrproxyclient.cpp
)
# CONSOLIDATED: the implementation imports the sibling streamr modules
# (the former internal header was merged into the .cpp), so the source
# needs module dependency scanning.
streamr_enable_imports(streamrproxyclient)

set_target_properties(streamrproxyclient PROPERTIES VERSION ${SHAREDLIB_VERSION} SOVERSION ${SHAREDLIB_SOVERSION})
# Sibling monorepo packages: find_package only when the target is not
# already in this build tree (in the root single-tree build the sibling
# was added via add_subdirectory — the guard makes the root tree
# self-contained, so it configures without the standalone per-package
# build dirs; MODERNIZATION.md "After the consolidation: CI speed").
if(NOT TARGET streamr::streamr-trackerless-network)
    find_package(streamr-trackerless-network CONFIG REQUIRED)
endif()
if(NOT TARGET streamr::streamr-dht)
    find_package(streamr-dht CONFIG REQUIRED)
endif()
if(NOT TARGET streamr::streamr-logger)
    find_package(streamr-logger CONFIG REQUIRED)
endif()
if(NOT TARGET streamr::streamr-utils)
    find_package(streamr-utils CONFIG REQUIRED)
endif()
if(NOT TARGET streamr::streamr-eventemitter)
    find_package(streamr-eventemitter CONFIG REQUIRED)
endif()
find_package(ada CONFIG REQUIRED)

target_include_directories(streamrproxyclient PUBLIC include)
target_link_libraries(streamrproxyclient
    PRIVATE streamr::streamr-trackerless-network
    PRIVATE streamr::streamr-logger
    PRIVATE streamr::streamr-dht
    PRIVATE streamr::streamr-utils
    PRIVATE streamr::streamr-eventemitter
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
     
    add_executable(streamr-streamrproxyclient-test-integration test/integration/StreamrProxyClientTest.cpp)
    # The test imports streamr.logger.SLogger (the former textual include
    # died with the logger consolidation, MODERNIZATION.md Phase 2.6).
    streamr_enable_imports(streamr-streamrproxyclient-test-integration)
    target_link_directories(streamr-streamrproxyclient-test-integration PUBLIC ${SHAREDLIB_OUTPUT_DIRECTORY})
    
    target_include_directories(streamr-streamrproxyclient-test-integration PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
    
    target_link_libraries(streamr-streamrproxyclient-test-integration 
        PRIVATE streamrproxyclient
        PRIVATE streamr::streamr-logger
        PRIVATE GTest::gtest 
        PRIVATE GTest::gtest_main
    )

    add_executable(streamr-streamrnode-test-integration test/integration/StreamrNodeTest.cpp)
    target_link_directories(streamr-streamrnode-test-integration PUBLIC ${SHAREDLIB_OUTPUT_DIRECTORY})

    target_include_directories(streamr-streamrnode-test-integration PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

    target_link_libraries(streamr-streamrnode-test-integration
        PRIVATE streamrproxyclient
        PRIVATE GTest::gtest
        PRIVATE GTest::gtest_main
    )

    # streamrNode* C API <-> TS network end-to-end test (milestone D):
    # drives the pinned @streamr/trackerless-network npm package in a
    # child node process (test/ts-interop/entryPointNode.js). Skips when
    # no node runtime >= 20 is available. Like the trackerless-network
    # interop test this is a manual-run target (not registered with
    # ctest): it downloads npm packages on first run.
    add_executable(streamr-streamrnode-test-ts-interop test/ts-interop/StreamrNodeTsInteropTest.cpp)
    target_compile_definitions(streamr-streamrnode-test-ts-interop
        PRIVATE TS_INTEROP_HARNESS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/ts-interop"
    )
    target_link_directories(streamr-streamrnode-test-ts-interop PUBLIC ${SHAREDLIB_OUTPUT_DIRECTORY})

    target_include_directories(streamr-streamrnode-test-ts-interop PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

    target_link_libraries(streamr-streamrnode-test-ts-interop
        PRIVATE streamrproxyclient
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

    add_executable(streamrproxyclient-cpp-wrapper-test  ${CMAKE_CURRENT_LIST_DIR}/wrappers/cpp/test/StreamrProxyClientCppWrapperTest.cpp)
    target_link_directories(streamrproxyclient-cpp-wrapper-test PUBLIC ${CMAKE_CURRENT_LIST_DIR}/build)

    target_include_directories(streamrproxyclient-cpp-wrapper-test PUBLIC ${CMAKE_CURRENT_LIST_DIR}/wrappers/cpp/include)

    target_link_libraries(streamrproxyclient-cpp-wrapper-test
        PRIVATE streamrproxyclient
        PRIVATE GTest::gtest
        PRIVATE GTest::gtest_main
    )

    add_executable(streamrnode-cpp-wrapper-test  ${CMAKE_CURRENT_LIST_DIR}/wrappers/cpp/test/StreamrNodeCppWrapperTest.cpp)
    target_link_directories(streamrnode-cpp-wrapper-test PUBLIC ${CMAKE_CURRENT_LIST_DIR}/build)

    target_include_directories(streamrnode-cpp-wrapper-test PUBLIC ${CMAKE_CURRENT_LIST_DIR}/wrappers/cpp/include)

    target_link_libraries(streamrnode-cpp-wrapper-test
        PRIVATE streamrproxyclient
        PRIVATE GTest::gtest
        PRIVATE GTest::gtest_main
    )

    if (NOT (${VCPKG_TARGET_TRIPLET} MATCHES "android"))
        include(GoogleTest)
        gtest_discover_tests(streamr-streamrproxyclient-test-integration)
        # The networked streamrNode* tests need more than the 25 s
        # default on the 3-core CI runners.
        gtest_discover_tests(streamr-streamrnode-test-integration PROPERTIES TIMEOUT 600)
        gtest_discover_tests(streamrproxyclient-cpp-wrapper-test)
        # Networked two-node round-trip, same budget as the streamrnode
        # integration tests.
        gtest_discover_tests(streamrnode-cpp-wrapper-test PROPERTIES TIMEOUT 600)
        # DISABLED: these tests build the whole streamr-dev/network TS
        # monorepo at a 2024 commit (ts-integration/install.sh), which no
        # longer works on current CI runners. The streamr network TS
        # packages have changed a lot since 2024 — the TS interop harness
        # will be rewritten in a separate project (see MODERNIZATION.md,
        # "Interim — ts-integration tests"). Kept as reference for that
        # rewrite.
        # add_test(
        #     NAME ts-end-to-end-test
        #     COMMAND "${CMAKE_CURRENT_LIST_DIR}/run-ts-end-to-end-tests.sh"
        #     WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        #     )

        # add_test(
        #     NAME ts-multiple-messages-test
        #     COMMAND "${CMAKE_CURRENT_LIST_DIR}/run-ts-multiple-messages-test.sh"
        #     WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        #     )
    endif()
    # Install headers to dist/target-triplet
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/streamrproxyclient.h
                  ${CMAKE_CURRENT_SOURCE_DIR}/include/streamrnode.h
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

        # Also create a python package and upload it to PyPi
        install(CODE "execute_process(COMMAND ${CMAKE_CURRENT_LIST_DIR}/createpythonpackage.sh ${CMAKE_CURRENT_SOURCE_DIR}/dist/${VCPKG_TARGET_TRIPLET}/lib/${CMAKE_BUILD_TYPE} ${VCPKG_TARGET_TRIPLET} ${CMAKE_CURRENT_LIST_DIR} ${STREAMRPROXYCLIENT_VERSION})")
    endif()
endif()

