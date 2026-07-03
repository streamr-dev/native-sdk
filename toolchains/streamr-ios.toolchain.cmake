# Wrapper around the vendored (unmodified) leetal/ios-cmake toolchain.
#
# A VCPKG_CHAINLOAD_TOOLCHAIN_FILE replaces vcpkg's own iOS toolchain — the
# component that would otherwise apply VCPKG_C_FLAGS / VCPKG_CXX_FLAGS /
# VCPKG_LINKER_FLAGS from the triplet. Consume them here instead, so triplet
# flags keep working for both vcpkg port builds (vcpkg passes the values as
# cache variables) and the SDK's own package builds (the triplet file is
# evaluated in the same configure and sets them as normal variables).
#
# ios.toolchain.cmake folds the pre-set CMAKE_*_FLAGS values into its own
# flag composition, and toolchain files are evaluated multiple times per
# configure, so these are plain overwrites (idempotent), not appends.
#
# Historical note: the previously vendored ios.toolchain.cmake carried this
# glue as in-file modifications reading ENV{VCPKG_*_FLAGS}; the wrapper
# replaces that so the upstream file can stay pristine and updatable.
set(CMAKE_C_FLAGS "${VCPKG_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${VCPKG_CXX_FLAGS}")
set(CMAKE_C_LINK_FLAGS "${VCPKG_LINKER_FLAGS}")
set(CMAKE_CXX_LINK_FLAGS "${VCPKG_LINKER_FLAGS}")
set(CMAKE_OBJC_LINK_FLAGS "${VCPKG_LINKER_FLAGS}")
set(CMAKE_OBJCXX_LINK_FLAGS "${VCPKG_LINKER_FLAGS}")

include("${CMAKE_CURRENT_LIST_DIR}/ios.toolchain.cmake")
