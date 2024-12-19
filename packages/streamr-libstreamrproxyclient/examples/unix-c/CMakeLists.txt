cmake_minimum_required(VERSION 3.26)

project(streamrproxyclientexample C)

set(CMAKE_BUILD_TYPE Release)

if(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(ARCH_DIR "arm64-osx")
    else()
        set(ARCH_DIR "x64-osx")
    endif()
elseif(UNIX AND NOT APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(ARCH_DIR "arm64-linux")
    else()
        set(ARCH_DIR "x64-linux")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

add_executable(publisherexample publisherexample.c)

target_include_directories(publisherexample PRIVATE ${CMAKE_SOURCE_DIR}/../../dist/${ARCH_DIR}/include)
target_link_directories(publisherexample PRIVATE ${CMAKE_SOURCE_DIR}/../../dist/${ARCH_DIR}/lib/${CMAKE_BUILD_TYPE})
target_link_libraries(publisherexample streamrproxyclient)
