set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# clang + libc++, matching the toolchain used for the SDK itself (see
# cmake/homebrewClang.cmake): one standard library across all platforms.
# This overlay also makes the self-hosted arm64 Linux runner use the same
# compiler as the x64 leg (previously it fell back to vcpkg's default).
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    -DCMAKE_C_COMPILER=clang-22
    -DCMAKE_CXX_COMPILER=clang++-22
    )
set(VCPKG_CXX_FLAGS "-stdlib=libc++")
set(VCPKG_C_FLAGS "")
set(VCPKG_LINKER_FLAGS "-stdlib=libc++")

if(${PORT} MATCHES "folly")
    message(STATUS "Port name is folly, setting build type to release")
    set(VCPKG_BUILD_TYPE release)
endif()

message(STATUS "OVERLAY TRIPLET arm64-linux loaded")
