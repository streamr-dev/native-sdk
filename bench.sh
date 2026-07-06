#!/bin/bash

# Compile-time benchmark harness for the C++ modules migration
# (MODERNIZATION.md Part 2). Measures the ROOT single-tree build — the
# workflow the migration optimizes (it compiles every package's tests, the
# generated protobuf sources and the proxy client in one Ninja graph).
# Results are recorded per phase in MODERNIZATION.md.
#
# Usage:
#   ./bench.sh clean         clean-build wall-clock (root tree only; keeps
#                            build/vcpkg_installed and the per-package build
#                            dirs — their exported configs are needed by the
#                            root configure)
#   ./bench.sh incremental [header]
#                            rebuild after touching a mid-stack module unit
#                            (default: streamr-utils modules/StreamID.cppm,
#                            imported across dht/trackerless-network — the
#                            headers were consolidated away in Phase 2.6)
#   ./bench.sh trace         clean build in a separate build-bench-trace/
#                            tree with -ftime-trace; if ClangBuildAnalyzer is
#                            installed (brew install clang-build-analyzer),
#                            prints the expensive-headers/templates report
#
# Methodology: run on an otherwise idle machine, 2-3 repetitions, take the
# best. All modes print a single "BENCH <mode>: ..." summary line at the end.

set -e

if [ -z "$VCPKG_ROOT" ]; then
    echo "Error: environment not set up. Run 'source install-prerequisities.sh' or 'source setenvs.sh' first."
    exit 1
fi

MODE="$1"
BUILD_TYPE="${BENCH_BUILD_TYPE:-Debug}"

print_env() {
    echo "bench environment:"
    echo "  host:        $(sysctl -n machdep.cpu.brand_string 2>/dev/null || grep -m1 'model name' /proc/cpuinfo | cut -d: -f2)"
    echo "  cores:       $(sysctl -n hw.ncpu 2>/dev/null || nproc)"
    echo "  parallelism: ${CMAKE_BUILD_PARALLEL_LEVEL:-ninja default}"
    echo "  compiler:    $(${LLVM_PREFIX:+$LLVM_PREFIX/bin/}clang++ --version | head -1)"
    echo "  cmake:       $(cmake --version | head -1)"
    echo "  build type:  $BUILD_TYPE"
}

configure_and_build() {
    # $1 = binary dir, remaining args = extra cmake configure options
    local BINDIR="$1"; shift
    local T_CONF_START T_CONF_END T_BUILD_END
    T_CONF_START=$(date +%s)
    cmake -B "$BINDIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE "$@" .
    T_CONF_END=$(date +%s)
    cmake --build "$BINDIR"
    T_BUILD_END=$(date +%s)
    CONFIGURE_SECONDS=$((T_CONF_END - T_CONF_START))
    BUILD_SECONDS=$((T_BUILD_END - T_CONF_END))
}

case "$MODE" in
  clean)
    print_env
    # Clean the root tree only: vcpkg_installed stays (dependencies are not
    # what is being measured), per-package build dirs stay (the root
    # configure resolves sibling packages against their exported configs).
    find build -mindepth 1 -maxdepth 1 ! -name vcpkg_installed ! -name .gitignore -exec rm -rf {} +
    configure_and_build build
    echo ""
    echo "BENCH clean: configure ${CONFIGURE_SECONDS}s, build ${BUILD_SECONDS}s (total $((CONFIGURE_SECONDS + BUILD_SECONDS))s)"
    ;;

  incremental)
    HEADER="${2:-packages/streamr-utils/modules/StreamID.cppm}"
    if [ ! -f "$HEADER" ]; then
        echo "Error: header not found: $HEADER"
        exit 1
    fi
    if [ ! -f build/build.ninja ]; then
        echo "Error: no configured root build tree; run ./bench.sh clean (or ./install.sh) first."
        exit 1
    fi
    print_env
    # Warm the graph so only the header touch is measured.
    cmake --build build
    touch "$HEADER"
    T_START=$(date +%s)
    cmake --build build
    T_END=$(date +%s)
    echo ""
    echo "BENCH incremental ($HEADER): $((T_END - T_START))s"
    ;;

  trace)
    print_env
    # Separate tree: -ftime-trace changes every object file, and the root
    # CMakeLists pins VCPKG_INSTALLED_DIR to build/vcpkg_installed, so a
    # second binary dir reuses the installed dependencies for free.
    rm -rf build-bench-trace
    configure_and_build build-bench-trace -DCMAKE_CXX_FLAGS=-ftime-trace
    echo ""
    echo "BENCH trace: configure ${CONFIGURE_SECONDS}s, build ${BUILD_SECONDS}s (with -ftime-trace)"
    if command -v ClangBuildAnalyzer >/dev/null 2>&1; then
        ClangBuildAnalyzer --all build-bench-trace bench-trace.bin
        ClangBuildAnalyzer --analyze bench-trace.bin
        rm -f bench-trace.bin
    else
        echo "ClangBuildAnalyzer not found (brew install clang-build-analyzer / build from"
        echo "https://github.com/aras-p/ClangBuildAnalyzer) — traces are in build-bench-trace/**/*.json"
    fi
    ;;

  *)
    echo "Usage: ./bench.sh clean | incremental [header] | trace"
    exit 1
    ;;
esac
