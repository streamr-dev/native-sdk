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

# Android builds use the NDK's clang. The modules build correctly with
# NDK r27+ (clang 18) — the failure previously attributed to compiler
# immaturity was a -pthread BMI configuration mismatch, fixed in the
# helpers below. Older NDKs are a hard error: the codebase is being
# consolidated into C++ modules (MODERNIZATION.md Phase 2.6), so the
# former fall-back of building textually from the internal headers no
# longer exists.
if(VCPKG_TARGET_TRIPLET MATCHES "android" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 18)
        set(STREAMR_MODULES_SUPPORTED ON)
    else()
        message(FATAL_ERROR
            "NDK clang ${CMAKE_CXX_COMPILER_VERSION} is too old: building "
            "this codebase requires C++ modules support (NDK r27+, "
            "clang >= 18). The pre-consolidation textual fall-back no "
            "longer exists (see MODERNIZATION.md Phase 2.6).")
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

# `import std` is MANDATORY: every module unit in this codebase imports the
# standard library as a module (`import std.compat;`), so the build must always
# provide it. CMake gates the standard-library module behind a per-CMake-version
# experimental UUID; the value below is for the CMake 4.3 series (see
# Help/dev/experimental.rst). A CMake upgrade requires bumping it — the build
# fails clearly if the UUID does not match the CMake in use.
#
# Per-platform prerequisites, applied by install.sh at configure time (they must
# be set before compiler detection, so they cannot live here):
#   - macOS/iOS (Homebrew LLVM libc++): -DCMAKE_CXX_STDLIB_MODULES_JSON=...,
#     because Homebrew's clang driver cannot locate its own libc++.modules.json.
#   - Android (NDK r29+): the std module compiles against Bionic only with
#     -D__BIONIC_CTYPE_INLINE=inline and -U_FORTIFY_SOURCE (Bionic ships
#     internal-linkage ctype/fortify inlines that a module cannot re-export),
#     plus --target=aarch64-none-linux-android<N> in CMAKE_CXX_FLAGS so CMake's
#     cross-compile stdlib probe detects libc++. -pthread is already PUBLIC on
#     the module targets below. NOTE: -U_FORTIFY_SOURCE disables a hardening
#     feature on shipped Android binaries — an accepted trade-off (see
#     MODERNIZATION.md, import std adoption).
if(STREAMR_MODULES_SUPPORTED)
    # Both CMAKE_CXX_MODULE_STD and the experimental UUID must be set BEFORE
    # project()/CXX enablement (the toolchain's import-std support is decided
    # then), so they are passed on the configure command line by install.sh, not
    # set here. Fail early with a clear message if a raw `cmake` invocation
    # forgot them.
    if(NOT DEFINED CMAKE_EXPERIMENTAL_CXX_IMPORT_STD OR NOT CMAKE_CXX_MODULE_STD)
        message(FATAL_ERROR
            "import std requires -DCMAKE_CXX_MODULE_STD=ON and "
            "-DCMAKE_EXPERIMENTAL_CXX_IMPORT_STD=<uuid> on the cmake command "
            "line (both must be set before project()). The UUID for the CMake "
            "4.3 series is 451f2fe2-a8a2-47c3-bc32-94786d8fc91b; install.sh "
            "passes both automatically.")
    endif()
endif()

# streamr_add_module_library(<target> FILES <unit.cppm>...)
#
# Defines <target> as a STATIC library whose C++ module interface units are
# the given files (FILE_SET CXX_MODULES, rooted at the package's modules/
# directory unless overridden with BASE_DIRS <dir> — e.g. test-only module
# units living under test/utils). STATIC rather than INTERFACE: module
# interface units are
# compiled TUs, so even a previously header-only package gains a compiled
# archive when it grows a module.
#
# Where modules are unsupported (Android/NDK), the target is still created
# as a STATIC library — with a generated stub source instead of module
# units — so callers can keep using PUBLIC include dirs/links identically.
function(streamr_add_module_library TARGET)
    cmake_parse_arguments(ARG "" "BASE_DIRS" "FILES" ${ARGN})
    if(NOT ARG_FILES)
        message(FATAL_ERROR "streamr_add_module_library(${TARGET}): FILES is required")
    endif()
    if(NOT ARG_BASE_DIRS)
        set(ARG_BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules)
    endif()
    add_library(${TARGET} STATIC)
    if(STREAMR_MODULES_SUPPORTED)
        target_sources(${TARGET}
            PUBLIC
            FILE_SET CXX_MODULES
            BASE_DIRS ${ARG_BASE_DIRS}
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
    # Importers must agree with the BMI on POSIX-thread support: clang
    # validates language options when loading a module file. On Android,
    # -pthread is not implied — test executables inherit it from GTest's
    # Threads::Threads while library targets do not, so a BMI built
    # without it cannot be loaded by an import-using test TU (observed:
    # -Wmodule-file-config-mismatch, then every imported name reported as
    # undeclared). Making the flag part of the PUBLIC interface keeps the
    # BMI and every importer consistent. No-op on other platforms.
    target_compile_options(${TARGET} PUBLIC $<$<PLATFORM_ID:Android>:-pthread>)
    # The module libraries' objects end up inside the shared
    # libstreamrproxyclient, so they must be position-independent. macOS
    # emits PIC by default; on Linux the ELF linker rejects the archive
    # otherwise ("recompile with -fPIC" against the module vtable/thunk
    # symbols — first seen when the consolidated code moved into module
    # objects on the linux-arm64 leg).
    set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()

# streamr_target_module_sources(<target> FILES <unit.cppm>...)
#
# Adds C++ module interface units to an EXISTING library target (used by
# packages that already compile ordinary sources, e.g. generated protobuf
# .cc files). Same effect as streamr_add_module_library() minus the
# add_library(). No-op where modules are unsupported.
function(streamr_target_module_sources TARGET)
    cmake_parse_arguments(ARG "" "BASE_DIRS" "FILES" ${ARGN})
    if(NOT ARG_FILES)
        message(FATAL_ERROR "streamr_target_module_sources(${TARGET}): FILES is required")
    endif()
    if(NOT ARG_BASE_DIRS)
        set(ARG_BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules)
    endif()
    if(NOT STREAMR_MODULES_SUPPORTED)
        return()
    endif()
    target_sources(${TARGET}
        PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${ARG_BASE_DIRS}
        FILES ${ARG_FILES})
    set_target_properties(${TARGET} PROPERTIES CXX_SCAN_FOR_MODULES ON)
    target_compile_features(${TARGET} PUBLIC cxx_std_26)
    # See streamr_add_module_library: BMI/importer -pthread consistency
    # and position independence for the shared-library link.
    target_compile_options(${TARGET} PUBLIC $<$<PLATFORM_ID:Android>:-pthread>)
    set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
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
