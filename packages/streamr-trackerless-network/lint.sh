#!/bin/bash

set -e

SRCFILES=$(find src -type f \( -name "*.hpp" -o -name "*.cpp" \) -not -path '*/proto/*' | sort | uniq | tr '\n' ' ')

if [ -n "$SRCFILES" ]; then
    # Compile database: the standalone build dir when present, else the root
# tree's (the Linux CI legs skip the standalone builds — MODERNIZATION.md
# "After the consolidation: CI speed" — and lint against the root tree,
# which compiles the same sources).
COMPILE_DB=./build
if [ ! -f ./build/compile_commands.json ]; then
    COMPILE_DB=../../build
fi

echo "Running clangd-tidy on $SRCFILES"
    clangd-tidy -p "$COMPILE_DB" $SRCFILES

    echo "Running clang-format --dry-run on $SRCFILES"
    ../../run-clang-format.py $SRCFILES
fi

# Two phase-C1 test files are excluded from clangd-tidy (owner-approved
# selective disabling pattern, see packages/streamr-dht/lint.sh for the
# full catalog of both false-positive classes):
# ContentDeliveryRpcRemoteTest.cpp trips the global-namespace
# duplicate-declaration merge failure (NetworkRpc/ProtoRpc types arrive
# both textually and through imported BMIs; member calls like
# requestid() are then diagnosed ambiguous), and
# TemporaryConnectionRpcLocalTest.cpp trips the std-type unification
# false positive (std::string-vs-std::string mismatch on its own
# locals). The compiler builds and runs both (unit suite green on every
# platform); clang-format still checks them.
# HandshakerTest.cpp (phase C2) trips the same std-type unification
# false positive inside an included header ("no viable conversion from
# const std::string to __self_view"); the compiler builds and runs it.
# ContentDeliveryLayerNodeTest.cpp (phase C3) trips the std-type
# unification false positive on its own std::string locals; the
# compiler builds and runs it.
TESTFILES=$(find test -type f \( -name "*.hpp" -o -name "*.cpp" \) -not -path '*/ts-integration/*' | sort | uniq | tr '\n' ' ')
TIDY_TESTFILES=$(echo "$TESTFILES" | tr ' ' '\n' | grep -v 'test/unit/ContentDeliveryRpcRemoteTest.cpp' | grep -v 'test/unit/TemporaryConnectionRpcLocalTest.cpp' | grep -v 'test/unit/HandshakerTest.cpp' | grep -v 'test/unit/ContentDeliveryLayerNodeTest.cpp' | tr '\n' ' ')
echo "Running clangd-tidy on $TIDY_TESTFILES"

clangd-tidy -p "$COMPILE_DB" $TIDY_TESTFILES < /dev/null

echo "Running clang-format --dry-run on $TESTFILES"
../../run-clang-format.py $TESTFILES

# CONSOLIDATED (MODERNIZATION.md Phase 2.6): the include/ tree is gone —
# the code lives in the module interface units, which are now linted
# with clangd-tidy as the source of truth (verified working with clangd
# 22; the consolidated units carry no export-using re-export blocks,
# which were the source of the earlier false positives).
# modules/gen/ holds protoc-plugin GENERATED module units (RPC stubs):
# excluded from linting like all generated code (see the src/proto
# exclusion above); the compiler builds them on every platform.
MODULE_FILES=$(find ./modules -type f -name "*.cppm" ! -path './modules/gen/*' 2>/dev/null | xargs echo)
if [ -n "$MODULE_FILES" ]; then
    echo "Running clangd-tidy on $MODULE_FILES"
    clangd-tidy -p "$COMPILE_DB" $MODULE_FILES

    echo "Running clang-format --dry-run on $MODULE_FILES"
    ../../run-clang-format.py $MODULE_FILES
fi
