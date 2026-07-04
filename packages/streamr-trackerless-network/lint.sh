#!/bin/bash

set -e

SRCFILES=$(find src -type f \( -name "*.hpp" -o -name "*.cpp" \) -not -path '*/proto/*' | sort | uniq | tr '\n' ' ')

if [ -n "$SRCFILES" ]; then
    echo "Running clangd-tidy on $SRCFILES"
    clangd-tidy -p ./build $SRCFILES

    echo "Running clang-format --dry-run on $SRCFILES"
    ../../run-clang-format.py $SRCFILES
fi

TESTFILES=$(find test -type f \( -name "*.hpp" -o -name "*.cpp" \) -not -path '*/ts-integration/*' | sort | uniq | tr '\n' ' ')
echo "Running clangd-tidy on $TESTFILES"

clangd-tidy -p ./build $TESTFILES

echo "Running clang-format --dry-run on $TESTFILES"
../../run-clang-format.py $TESTFILES

# CONSOLIDATED (MODERNIZATION.md Phase 2.6): the include/ tree is gone —
# the code lives in the module interface units, which are now linted
# with clangd-tidy as the source of truth (verified working with clangd
# 22; the consolidated units carry no export-using re-export blocks,
# which were the source of the earlier false positives).
MODULE_FILES=$(find ./modules -type f -name "*.cppm" 2>/dev/null | xargs echo)
if [ -n "$MODULE_FILES" ]; then
    echo "Running clangd-tidy on $MODULE_FILES"
    clangd-tidy -p ./build $MODULE_FILES

    echo "Running clang-format --dry-run on $MODULE_FILES"
    ../../run-clang-format.py $MODULE_FILES
fi
