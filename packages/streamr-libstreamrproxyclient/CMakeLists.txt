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

project(streamr-libstreamrproxyclient CXX)
add_library(libstreamrproxyclient SHARED
            src/libstreamrproxyclient.cpp
            include/libstreamrproxyclient.hpp
)

target_include_directories(libstreamrproxyclient PUBLIC include)

if (IOS)
    message(STATUS "Building dylib for iOS")
    target_link_libraries(libstreamrproxyclient ${FOUNDATION_LIBRARY})
    target_compile_definitions(libstreamrproxyclient PUBLIC IS_BUILDING_SHARED)
    set_target_properties(libstreamrproxyclient PROPERTIES
        FRAMEWORK TRUE
        FRAMEWORK_VERSION A
        MACOSX_FRAMEWORK_IDENTIFIER network.streamr.streamrproxyclient
        VERSION 1.0.0
        SOVERSION 1.0.0
        PUBLIC_HEADER include/libstreamrproxyclient.hpp
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Petri Savolainen"
    )
    install(CODE "execute_process(COMMAND ${CMAKE_CURRENT_LIST_DIR}/createiosframework.sh)")
endif()

