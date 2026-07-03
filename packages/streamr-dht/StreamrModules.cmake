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

# CMake's C++ modules support requires the Ninja generator (Makefiles cannot
# express the dynamically discovered module dependency graph).
if(NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(FATAL_ERROR
        "C++ modules require the Ninja generator; current generator is "
        "'${CMAKE_GENERATOR}'. install-prerequisities.sh/setenvs.sh export "
        "CMAKE_GENERATOR=Ninja — source one of them (and ./clean.sh once if "
        "the build dir was configured with another generator).")
endif()

# AppleClang has no CMake modules support; the build already uses Homebrew
# LLVM everywhere (homebrewClang.cmake), so this only guards misconfiguration.
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    message(FATAL_ERROR
        "CMake does not support C++ modules with AppleClang. The build is "
        "expected to use Homebrew LLVM (see homebrewClang.cmake / "
        "LLVM_PREFIX).")
endif()

# CMP0155 NEW: scan C++20+ sources for module dependencies by default.
# Scanning stays globally disabled via CMAKE_CXX_SCAN_FOR_MODULES OFF until
# a target opts in through the helpers below.
cmake_policy(SET CMP0155 NEW)

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
function(streamr_add_module_library TARGET)
    cmake_parse_arguments(ARG "" "" "FILES" ${ARGN})
    if(NOT ARG_FILES)
        message(FATAL_ERROR "streamr_add_module_library(${TARGET}): FILES is required")
    endif()
    add_library(${TARGET} STATIC)
    target_sources(${TARGET}
        PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
        FILES ${ARG_FILES})
    set_target_properties(${TARGET} PROPERTIES CXX_SCAN_FOR_MODULES ON)
    # PUBLIC compile feature (not just CMAKE_CXX_STANDARD): when another
    # build tree imports this target from its export, CMake synthesizes a
    # BMI-compiling target on the consumer side and requires the standard
    # level to be part of the exported usage requirements.
    target_compile_features(${TARGET} PUBLIC cxx_std_26)
endfunction()

# streamr_enable_imports(<target>)
#
# Enables module scanning on an existing target whose ordinary .cpp sources
# use `import` (e.g. a test executable of a migrated package). Without this
# the global CMAKE_CXX_SCAN_FOR_MODULES OFF would leave the import edges
# undiscovered and the build would race the BMIs.
function(streamr_enable_imports TARGET)
    set_target_properties(${TARGET} PROPERTIES CXX_SCAN_FOR_MODULES ON)
endfunction()
