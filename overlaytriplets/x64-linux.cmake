set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

if(${PORT} MATCHES "folly")
    message(STATUS "Port name is folly, setting build type to release")
    set(VCPKG_BUILD_TYPE release)
endif()

message(STATUS "OVERLAY TRIPLET x64-linux loaded")
