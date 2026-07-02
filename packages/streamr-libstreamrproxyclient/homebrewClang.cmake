# CANONICAL COPY — the per-package copies in packages/*/homebrewClang.cmake
# are generated from this file by ./sync-cmake-files.sh. Edit THIS file and
# run the sync script; do not edit the package copies directly.
# (Each package carries its own copy so that it can be published as a
# standalone vcpkg package later.)

if (APPLE AND NOT (VCPKG_TARGET_TRIPLET MATCHES "android" OR VCPKG_TARGET_TRIPLET MATCHES "linux"))
    message(STATUS "APPLE is defined")
    set(HOMEBREW_PREFIX $ENV{HOMEBREW_PREFIX})

    # Homebrew's llvm formula is keg-only (not linked into
    # ${HOMEBREW_PREFIX}/bin). install-prerequisities.sh exports
    # LLVM_PREFIX=$(brew --prefix llvm); fall back to the conventional keg
    # location if the variable is not set.
    if (DEFINED ENV{LLVM_PREFIX})
        set(LLVM_PREFIX "$ENV{LLVM_PREFIX}")
    else()
        set(LLVM_PREFIX "${HOMEBREW_PREFIX}/opt/llvm")
    endif()
    message(STATUS "Using LLVM from ${LLVM_PREFIX}")

    set(CMAKE_C_COMPILER "${LLVM_PREFIX}/bin/clang")
    set(CMAKE_CXX_COMPILER "${LLVM_PREFIX}/bin/clang++")
    set(ENV{CC} "${CMAKE_C_COMPILER}")
    set(ENV{CXX} "${CMAKE_CXX_COMPILER}")


    set(CMAKE_PREFIX_PATH
        "${LLVM_PREFIX}"
        "${HOMEBREW_PREFIX}"
    )

    list(TRANSFORM CMAKE_PREFIX_PATH APPEND "/include"
         OUTPUT_VARIABLE CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES)
    set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES "${CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES}")

    if (NOT (VCPKG_TARGET_TRIPLET MATCHES "ios"))
        set(CMAKE_FIND_FRAMEWORK LAST)
        set(CMAKE_FIND_APPBUNDLE LAST)
        add_link_options("-L${LLVM_PREFIX}/lib/c++" "-Wl,-rpath,${LLVM_PREFIX}/lib/c++")
    endif()

    if (VCPKG_TARGET_TRIPLET MATCHES "ios")
        set(CMAKE_FIND_FRAMEWORK FIRST)
        set(CMAKE_FIND_APPBUNDLE FIRST)
    endif()
elseif (UNIX AND NOT APPLE AND NOT (VCPKG_TARGET_TRIPLET MATCHES "android"))
    # Linux builds use clang + libc++ (CC/CXX are exported by
    # install-prerequisities.sh). libc++ keeps the standard library uniform
    # across macOS, iOS, Android (NDK) and Linux — one C++26 feature matrix
    # and a single C++ modules implementation to support.
    add_compile_options(-stdlib=libc++)
    add_link_options(-stdlib=libc++)
endif()
