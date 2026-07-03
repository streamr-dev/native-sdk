# CANONICAL COPY — the per-package copies in packages/*/StreamrModules.cmake
# are generated from this file by ./sync-cmake-files.sh. Edit THIS file and
# run the sync script; do not edit the package copies directly.
# (Each package carries its own copy so that it can be published as a
# standalone vcpkg package later.)
#
# C++ named-modules build support (MODERNIZATION.md Part 2). A package
# includes this file when it starts defining module targets. Non-migrated
# packages are unaffected: the root build keeps CMAKE_CXX_SCAN_FOR_MODULES
# OFF globally (clean compile commands for clangd), and the helpers below
# re-enable scanning per target.

# Android builds use the NDK's clang, and modules support depends on its
# version: NDK r27 ships clang 18, whose C++ modules support is too
# immature for the façade modules (known `export using` overload-set bugs —
# observed: `import streamr.json;` failed to provide the toJson overloads).
# NDK r29 ships clang 21, which is expected to handle them. Gate on the
# compiler version: with an older NDK, Android consumes the ordinary
# headers instead (they remain the source of truth during the façade
# stage), the module units are skipped, and import-using test/example
# targets are not built (STREAMR_MODULES_SUPPORTED guards them).
if(VCPKG_TARGET_TRIPLET MATCHES "android" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 21)
        set(STREAMR_MODULES_SUPPORTED ON)
    else()
        set(STREAMR_MODULES_SUPPORTED OFF)
        message(STATUS
            "C++ modules disabled: NDK clang ${CMAKE_CXX_COMPILER_VERSION} "
            "< 21 (use NDK r29+ for modules on Android)")
    endif()
else()
    set(STREAMR_MODULES_SUPPORTED ON)
endif()

if(STREAMR_MODULES_SUPPORTED)
    # CMake's C++ modules support requires the Ninja generator (Makefiles
    # cannot express the dynamically discovered module dependency graph).
    if(NOT CMAKE_GENERATOR MATCHES "Ninja")
        message(FATAL_ERROR
            "C++ modules require the Ninja generator; current generator is "
            "'${CMAKE_GENERATOR}'. install-prerequisities.sh/setenvs.sh export "
            "CMAKE_GENERATOR=Ninja — source one of them (and ./clean.sh once if "
            "the build dir was configured with another generator).")
    endif()

    # AppleClang has no CMake modules support; the build already uses Homebrew
    # LLVM everywhere (homebrewClang.cmake), so this only guards
    # misconfiguration.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        message(FATAL_ERROR
            "CMake does not support C++ modules with AppleClang. The build is "
            "expected to use Homebrew LLVM (see homebrewClang.cmake / "
            "LLVM_PREFIX).")
    endif()

    # CMP0155 NEW: scan C++20+ sources for module dependencies by default.
    # Scanning stays globally disabled via CMAKE_CXX_SCAN_FOR_MODULES OFF
    # until a target opts in through the helpers below.
    cmake_policy(SET CMP0155 NEW)
endif()

# Opt-in `import std;` (experimental in CMake, OFF by default — see the
# MODERNIZATION.md decision: no rollout now). CMake gates the feature behind
# a per-version experimental UUID, so turning this on requires passing the
# UUID for the CMake version in use:
#   cmake -DSTREAMR_IMPORT_STD=ON \
#         -DCMAKE_EXPERIMENTAL_CXX_IMPORT_STD=<uuid-for-your-cmake> ...
option(STREAMR_IMPORT_STD "Build with experimental 'import std' support" OFF)
if(STREAMR_IMPORT_STD)
    if(NOT DEFINED CMAKE_EXPERIMENTAL_CXX_IMPORT_STD)
        message(FATAL_ERROR
            "STREAMR_IMPORT_STD=ON needs "
            "-DCMAKE_EXPERIMENTAL_CXX_IMPORT_STD=<uuid> (the experimental "
            "feature UUID documented for your CMake version in "
            "Help/dev/experimental.rst of the CMake source).")
    endif()
    set(CMAKE_CXX_MODULE_STD ON)
endif()

# streamr_add_module_library(<target> FILES <unit.cppm>...)
#
# Defines <target> as a STATIC library whose C++ module interface units are
# the given files (FILE_SET CXX_MODULES, rooted at the package's modules/
# directory). STATIC rather than INTERFACE: module interface units are
# compiled TUs, so even a previously header-only package gains a compiled
# archive when it grows a module.
#
# Where modules are unsupported (Android/NDK), the target is still created
# as a STATIC library — with a generated stub source instead of module
# units — so callers can keep using PUBLIC include dirs/links identically.
function(streamr_add_module_library TARGET)
    cmake_parse_arguments(ARG "" "" "FILES" ${ARGN})
    if(NOT ARG_FILES)
        message(FATAL_ERROR "streamr_add_module_library(${TARGET}): FILES is required")
    endif()
    add_library(${TARGET} STATIC)
    if(STREAMR_MODULES_SUPPORTED)
        target_sources(${TARGET}
            PUBLIC
            FILE_SET CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
            FILES ${ARG_FILES})
        set_target_properties(${TARGET} PROPERTIES CXX_SCAN_FOR_MODULES ON)
    else()
        set(STUB ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-no-modules-stub.cpp)
        file(WRITE ${STUB}
            "// C++ modules are not built on this platform (see StreamrModules.cmake).\n")
        target_sources(${TARGET} PRIVATE ${STUB})
    endif()
    # PUBLIC compile feature (not just CMAKE_CXX_STANDARD): when another
    # build tree imports this target from its export, CMake synthesizes a
    # BMI-compiling target on the consumer side and requires the standard
    # level to be part of the exported usage requirements.
    target_compile_features(${TARGET} PUBLIC cxx_std_26)
endfunction()

# streamr_target_module_sources(<target> FILES <unit.cppm>...)
#
# Adds C++ module interface units to an EXISTING library target (used by
# packages that already compile ordinary sources, e.g. generated protobuf
# .cc files). Same effect as streamr_add_module_library() minus the
# add_library(). No-op where modules are unsupported.
function(streamr_target_module_sources TARGET)
    cmake_parse_arguments(ARG "" "" "FILES" ${ARGN})
    if(NOT ARG_FILES)
        message(FATAL_ERROR "streamr_target_module_sources(${TARGET}): FILES is required")
    endif()
    if(NOT STREAMR_MODULES_SUPPORTED)
        return()
    endif()
    target_sources(${TARGET}
        PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
        FILES ${ARG_FILES})
    set_target_properties(${TARGET} PROPERTIES CXX_SCAN_FOR_MODULES ON)
    target_compile_features(${TARGET} PUBLIC cxx_std_26)
endfunction()

# streamr_enable_imports(<target>)
#
# Enables module scanning on an existing target whose ordinary .cpp sources
# use `import` (e.g. a test executable of a migrated package). Without this
# the global CMAKE_CXX_SCAN_FOR_MODULES OFF would leave the import edges
# undiscovered and the build would race the BMIs.
function(streamr_enable_imports TARGET)
    if(NOT STREAMR_MODULES_SUPPORTED)
        return()
    endif()
    set_target_properties(${TARGET} PROPERTIES CXX_SCAN_FOR_MODULES ON)
endfunction()
