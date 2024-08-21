
#own stuff below
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME iOS)

set(CMAKE_CXX_STANDARD 26)
set(HOMEBREW_PREFIX $ENV{HOMEBREW_PREFIX})

set(LLVM_PREFIX "${HOMEBREW_PREFIX}")
set(CMAKE_C_COMPILER "${LLVM_PREFIX}/bin/clang")
set(CMAKE_CXX_COMPILER "${LLVM_PREFIX}/bin/clang++")
set(ENV{CC} "${CMAKE_C_COMPILER}")
set(ENV{CXX} "${CMAKE_CXX_COMPILER}")

set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DCMAKE_C_COMPILER=${LLVM_PREFIX}/bin/clang -DCMAKE_CXX_COMPILER=${LLVM_PREFIX}/bin/clang++ -DCMAKE_CXX_STANDARD=26 -DFOLLY_HAVE_CLOCK_GETTIME=1 -DFOLLY_MOBILE=0 -DIS_AARCH64_ARCH=0 -D__APPLE__=1 -DFOLLY_HAVE_MALLOC_USABLE_SIZE=0 -DPLATFORM=OS64)

set(VCPKG_CXX_FLAGS "-isystem /opt/homebrew/include/c++/v1 -DFOLLY_MOBILE=0")
set(VCPKG_C_FLAGS "-isystem /opt/homebrew/include/c++/v1 -DFOLLY_MOBILE=0")

set(VCPKG_LINKER_FLAGS "-lc++abi")

set(ENV{VCPKG_CXX_FLAGS} "${VCPKG_CXX_FLAGS}")
set(ENV{VCPKG_C_FLAGS} "${VCPKG_C_FLAGS}")

#set(ENV{PLATFORM} "SIMULATORARM64")
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../toolchains/ios.toolchain.cmake")