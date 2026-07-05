#!/bin/bash

set -e

FILES=$(find . -type d \( -name src -o -name test -o -name include \) ! -path '*/build/*' ! -path '*/proto/*' -print0 | xargs -0 -I{} find {} -type f \( -name "*.hpp" -o -name "*.cpp" \) -print0 | xargs -0 echo)
echo "Running clangd-tidy on $FILES"

# ConnectionLockingTest.cpp is excluded from clangd-tidy (owner-approved
# selective disabling, Phase 2.6 consolidation): it instantiates the
# imported streamr::utils::collect, whose folly internals clangd cannot
# resolve through module interfaces (spurious WithAsyncStackFunction
# error). The COMPILER accepts the file; clang-format still checks it.
TIDY_FILES=$(echo "$FILES" | tr ' ' '\n' | grep -v 'test/integration/ConnectionLockingTest.cpp' | tr '\n' ' ')
clangd-tidy -p ./build $TIDY_FILES

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
    clangd-tidy -p ./build $MODULE_FILES

    echo "Running clang-format --dry-run on $MODULE_FILES"
    ../../run-clang-format.py $MODULE_FILES
fi
