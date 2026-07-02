set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# clang + libc++, matching the toolchain used for the SDK itself (see
# cmake/homebrewClang.cmake): one standard library across all platforms.
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    -DCMAKE_C_COMPILER=clang-22
    -DCMAKE_CXX_COMPILER=clang++-22
    -DCMAKE_CXX_STANDARD=26
    )
set(VCPKG_CXX_FLAGS "-stdlib=libc++")
set(VCPKG_C_FLAGS "")
set(VCPKG_LINKER_FLAGS "-stdlib=libc++")

if(${PORT} MATCHES "folly")
    message(STATUS "Port name is folly, setting build type to release")
    set(VCPKG_BUILD_TYPE release)
endif()

message(STATUS "OVERLAY TRIPLET x64-linux loaded")
