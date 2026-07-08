set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME iOS)

set(CMAKE_CXX_STANDARD 26)
set(HOMEBREW_PREFIX $ENV{HOMEBREW_PREFIX})

# Homebrew llvm is keg-only; install-prerequisities.sh exports LLVM_PREFIX.
if(DEFINED ENV{LLVM_PREFIX})
    set(LLVM_PREFIX "$ENV{LLVM_PREFIX}")
else()
    set(LLVM_PREFIX "${HOMEBREW_PREFIX}/opt/llvm")
endif()
set(CMAKE_C_COMPILER "${LLVM_PREFIX}/bin/clang")
set(CMAKE_CXX_COMPILER "${LLVM_PREFIX}/bin/clang++")
set(ENV{CC} "${CMAKE_C_COMPILER}")
set(ENV{CXX} "${CMAKE_CXX_COMPILER}")

# iOS deployment target. Product decision (2026-07): iPhones keep themselves
# up to date, so the SDK targets the current major. The availability
# annotations in the SDK's libc++ headers follow this value.
set(DEPLOYMENT_TARGET "26.0")

# TESTING (2026-07-08, pre-import-std validation): use HOMEBREW LLVM's libc++
# HEADERS (not the iOS SDK's), linking against the device's libc++ RUNTIME.
# This is the arrangement `import std` needs on iOS — the iOS SDK libc++ ships
# no std module, Homebrew LLVM 22's does. It reverts the Phase 1.4 switch to
# SDK libc++. At DEPLOYMENT_TARGET 26.0 the old availability hacks
# (-D_LIBCPP_AVAILABILITY_HAS_INIT_PRIMARY_EXCEPTION=0, -lc++abi) are no longer
# required: __cxa_init_primary_exception shipped in the device libc++ at
# iOS 18.4, far below 26.0 (compile+link verified; gated on a real-device
# iostest run before adopting).
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    -DCMAKE_C_COMPILER=${LLVM_PREFIX}/bin/clang
    -DCMAKE_CXX_COMPILER=${LLVM_PREFIX}/bin/clang++
    -DCMAKE_CXX_STANDARD=26
    -DPLATFORM=OS64
    -DDEPLOYMENT_TARGET=${DEPLOYMENT_TARGET})

set(VCPKG_CXX_FLAGS "-isystem ${LLVM_PREFIX}/include/c++/v1")
set(VCPKG_C_FLAGS "")

if(${PORT} MATCHES "usrsctp")
    set(VCPKG_CXX_FLAGS "${VCPKG_CXX_FLAGS} -D__APPLE_USE_RFC_2292")
    set(VCPKG_C_FLAGS "${VCPKG_C_FLAGS} -D__APPLE_USE_RFC_2292")
endif()

if(${PORT} MATCHES "folly")
    # folly's configure uses try_run() checks, which cannot execute when
    # cross-compiling; preset their results (same approach as
    # arm64-android.cmake). The values mirror what the checks detect when
    # they actually run on an arm64 Apple host (arm64-osx build).
    # malloc_usable_size does not exist on iOS (only malloc_size), but
    # folly's link check false-positives against the SDK stubs — pin it off
    # or small_vector.h fails to compile.
    # IS_AARCH64_ARCH=0: the iOS toolchain reports CMAKE_SYSTEM_PROCESSOR
    # "aarch64" (macOS host says "arm64"), which enables folly's
    # Arm-Optimized-Routines assembly memcpy — ELF-only directives that the
    # Mach-O assembler rejects. Pin off to use folly's portable memcpy,
    # matching the macOS host build.
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS ${VCPKG_CMAKE_CONFIGURE_OPTIONS}
        -DFOLLY_HAVE_MALLOC_USABLE_SIZE=0
        -DIS_AARCH64_ARCH=0
        -DHAVE_VSNPRINTF_ERRORS_EXITCODE=1
        -DHAVE_VSNPRINTF_ERRORS_EXITCODE__TRYRUN_OUTPUT=a
        -DFOLLY_HAVE_WCHAR_SUPPORT_EXITCODE=0
        -DFOLLY_HAVE_WCHAR_SUPPORT_EXITCODE__TRYRUN_OUTPUT=a
        -DFOLLY_HAVE_LINUX_VDSO_EXITCODE=1
        -DFOLLY_HAVE_LINUX_VDSO_EXITCODE__TRYRUN_OUTPUT=a
        -DFOLLY_HAVE_UNALIGNED_ACCESS_EXITCODE=0
        -DFOLLY_HAVE_UNALIGNED_ACCESS_EXITCODE__TRYRUN_OUTPUT=a
        -DFOLLY_HAVE_WEAK_SYMBOLS_EXITCODE=1
        -DFOLLY_HAVE_WEAK_SYMBOLS_EXITCODE__TRYRUN_OUTPUT=a)
endif()

set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../toolchains/streamr-ios.toolchain.cmake")
