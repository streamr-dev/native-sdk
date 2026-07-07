#!/bin/bash

set -e

FILES=$(find . -type d \( -name src -o -name test -o -name include \) ! -path '*/build/*' ! -path '*/proto/*' -print0 | xargs -0 -I{} find {} -type f \( -name "*.hpp" -o -name "*.cpp" \) -print0 | xargs -0 echo)
# Compile database: the standalone build dir when present, else the root
# tree's (the Linux CI legs skip the standalone builds — MODERNIZATION.md
# "After the consolidation: CI speed" — and lint against the root tree,
# which compiles the same sources).
COMPILE_DB=./build
if [ ! -f ./build/compile_commands.json ]; then
    COMPILE_DB=../../build
fi

echo "Running clangd-tidy on $FILES"

# ConnectionLockingTest.cpp is excluded from clangd-tidy (owner-approved
# selective disabling, Phase 2.6 consolidation): it instantiates the
# imported streamr::utils::collect, whose folly internals clangd cannot
# resolve through module interfaces (spurious WithAsyncStackFunction
# error). The COMPILER accepts the file; clang-format still checks it.
# PendingConnectionTest.cpp joined the exclusion list with the
# third-party wrapper modules (streamr.utils.CoroutineHelper): its import
# set changed and clangd now trips the same preamble/BMI std-type
# unification false positive (spurious std::string-vs-std::string
# mismatch). The compiler accepts and runs the file on every platform.
# SimulatorTest.cpp, SimultaneousConnectionsTest.cpp and
# ConnectionManagerIntegrationTest.cpp (phase 0.3, from-scratch network
# simulator + ported TS integration tests) trip the same false positive
# through their simulator-module imports. The compiler accepts and runs
# all three on every platform; clang-format still checks them.
# DhtNodeRpcLocalTest.cpp and DhtNodeRpcRemoteTest.cpp (phase A3, the
# DhtNodeRpc client/server port) trip the same std-type unification false
# positive through the DhtNodeRpc module cluster (DhtNodeRpcRemote /
# DhtRpcClient imports): clangd reports gtest's own CodeLocation /
# MakeAndRegisterTestInfo calls as ambiguous (spurious two-std::string
# mismatch). The compiler accepts and runs both on every platform;
# clang-format still checks them. (PeerManagerTest.cpp imports the same
# cluster but does NOT trip it, so it stays in the clangd-tidy set.)
TIDY_FILES=$(echo "$FILES" | tr ' ' '\n' | grep -v 'test/integration/ConnectionLockingTest.cpp' | grep -v 'test/unit/PendingConnectionTest.cpp' | grep -v 'test/unit/SimulatorTest.cpp' | grep -v 'test/integration/SimultaneousConnectionsTest.cpp' | grep -v 'test/integration/ConnectionManagerIntegrationTest.cpp' | grep -v 'test/unit/DhtNodeRpcLocalTest.cpp' | grep -v 'test/integration/DhtNodeRpcRemoteTest.cpp' | tr '\n' ' ')
clangd-tidy -p "$COMPILE_DB" $TIDY_FILES

echo "Running clang-format --dry-run on $FILES"
../../run-clang-format.py $FILES

# CONSOLIDATED (MODERNIZATION.md Phase 2.6): the include/ tree is gone —
# the code lives in the module interface units, which are linted with
# clangd-tidy as the source of truth (the consolidated units carry no
# export-using re-export blocks, which caused the earlier clangd false
# positives).
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
