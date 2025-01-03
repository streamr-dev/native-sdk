set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES x86_64)

set(CMAKE_CXX_STANDARD 26)
set(HOMEBREW_PREFIX $ENV{HOMEBREW_PREFIX})

set(LLVM_PREFIX "${HOMEBREW_PREFIX}")
set(CMAKE_C_COMPILER "${LLVM_PREFIX}/bin/clang")
set(CMAKE_CXX_COMPILER "${LLVM_PREFIX}/bin/clang++")
set(ENV{CC} "${CMAKE_C_COMPILER}")
set(ENV{CXX} "${CMAKE_CXX_COMPILER}")

set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DCMAKE_C_COMPILER=${LLVM_PREFIX}/bin/clang -DCMAKE_CXX_COMPILER=${LLVM_PREFIX}/bin/clang++ -DCMAKE_CXX_STANDARD=26)

set(VCPKG_CXX_FLAGS "-isystem /opt/homebrew/include/c++/v1")
set(VCPKG_C_FLAGS "-isystem /opt/homebrew/include/c++/v1")
set(VCPKG_LINKER_FLAGS "-L/opt/homebrew/lib/c++ -Wl,-rpath,/opt/homebrew/lib/c++")

message(STATUS "OVERLAY TRIPLET x64-osx loaded")
