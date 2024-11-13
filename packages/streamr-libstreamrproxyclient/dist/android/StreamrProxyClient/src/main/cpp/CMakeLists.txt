
# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("ProxyClient")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.


set(STREAMR_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../build/libstreamrproxyclient.so")

message(STATUS "Looking for libstreamrproxyclient.so at: ${STREAMR_LIB_PATH}")

# Print the path for debugging
message(STATUS "Looking for libstreamrproxyclient.so at: ${STREAMR_LIB_PATH}")

# Verify the file exists
if(NOT EXISTS ${STREAMR_LIB_PATH})
    message(FATAL_ERROR "Cannot find libstreamrproxyclient.so at: ${STREAMR_LIB_PATH}")
endif()

add_library(ProxyClient SHARED
        ProxyClient.cpp
)

add_library(streamrproxyclient SHARED IMPORTED)
set_target_properties(streamrproxyclient PROPERTIES
        IMPORTED_LOCATION ${STREAMR_LIB_PATH}
)

include_directories(${CMAKE_SOURCE_DIR})

target_link_libraries(${CMAKE_PROJECT_NAME}
        android
        log
        streamrproxyclient
)

