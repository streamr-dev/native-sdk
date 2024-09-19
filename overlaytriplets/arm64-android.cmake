set(HAVE_FLAG_SEARCH_PATHS_FIRST 0)
set(CMAKE_C_LINK_FLAGS "")
set(CMAKE_CXX_LINK_FLAGS "")

set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_TARGET_TRIPLET arm64-android)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_MAKE_BUILD_TRIPLET "--host=aarch64-linux-android")
set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DANDROID_ABI=arm64-v8a -DCMAKE_CXX_STANDARD=26)

#set(CRT_LINKAGE static)
#set(LIBRARY_LINKAGE static)
set(TARGET_ARCHITECTURE arm64)
#set(CMAKE_SYSTEM_NAME Android)
#set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
set(ANDROID_PLATFORM 24)
set(ANDROID_NATIVE_API_LEVEL 24)

# Disable tryrun() checks for folly to enable cross compilation
if(${PORT} MATCHES "folly")
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS} -DHAVE_VSNPRINTF_ERRORS_EXITCODE=0)
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS} -DHAVE_VSNPRINTF_ERRORS_EXITCODE__TRYRUN_OUTPUT=a)
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS} -DFOLLY_HAVE_WCHAR_SUPPORT_EXITCODE=0)
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS} -DFOLLY_HAVE_WCHAR_SUPPORT_EXITCODE__TRYRUN_OUTPUT=a)
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS} -DFOLLY_HAVE_LINUX_VDSO_EXITCODE=1)
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS} -DFOLLY_HAVE_LINUX_VDSO_EXITCODE__TRYRUN_OUTPUT=a)
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS} -DFOLLY_HAVE_UNALIGNED_ACCESS_EXITCODE=0)
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS} -DFOLLY_HAVE_UNALIGNED_ACCESS_EXITCODE__TRYRUN_OUTPUT=a)
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS} -DFOLLY_HAVE_WEAK_SYMBOLS_EXITCODE=1)
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS} -DFOLLY_HAVE_WEAK_SYMBOLS_EXITCODE__TRYRUN_OUTPUT=a)
endif()

set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS} -DCMAKE_POLICY_DEFAULT_CMP0057=NEW -DCMAKE_POLICY_CMP0057=NEW)
set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS} -DANDROID_NATIVE_API_LEVEL=24 -DANDROID_PLATFORM=24)

set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "$ENV{ANDROID_NDK}/build/cmake/android.toolchain.cmake")

message(STATUS "OVERLAY TRIPLET arm64-android loaded")