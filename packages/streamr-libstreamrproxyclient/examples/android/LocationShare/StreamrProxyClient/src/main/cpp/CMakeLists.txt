cmake_minimum_required(VERSION 3.22.1)
project(ProxyClient)

# Set the correct relative path to the library
set(STREAMR_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../build/libstreamrproxyclient.so")

message(STATUS "Looking for libstreamrproxyclient.so at: ${STREAMR_LIB_PATH}")

# Add the prebuilt library
add_library(streamrproxyclient SHARED IMPORTED)
set_target_properties(streamrproxyclient PROPERTIES IMPORTED_LOCATION
        ${STREAMR_LIB_PATH})

# Add your library
add_library(ProxyClient SHARED
        ProxyClient.cpp
)

# Find the Android log library
find_library(log-lib log)

# Link libraries
target_link_libraries(ProxyClient
        streamrproxyclient
        ${log-lib}
)
