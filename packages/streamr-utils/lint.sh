#!/bin/bash

set -e

FILES=$(find . -type d \( -name src -o -name include -o -name test \) ! -path '*/build/*' ! -path '*/proto/*' -print0 | xargs -0 -I{} find {} -type f \( -name "*.hpp" -o -name "*.cpp" \) -print0 | xargs -0 echo)
# Compile database: the standalone build dir when present, else the root
# tree's (the Linux CI legs skip the standalone builds — MODERNIZATION.md
# "After the consolidation: CI speed" — and lint against the root tree,
# which compiles the same sources).
COMPILE_DB=./build
if [ ! -f ./build/compile_commands.json ]; then
    COMPILE_DB=../../build
fi

echo "Running clangd-tidy on $FILES"

# toEthereumAddressOrENSNameTest.cpp: clangd's experimental modules
# support cannot unify std types between its preamble and the module BMIs
# in this file (std::string crosses the module boundary in both
# directions) — excluded from clangd-tidy until clangd modules support
# matures. The compiler still fully typechecks it on every build and
# clang-format still covers it.
TIDY_FILES=$(echo "$FILES" | tr ' ' '\n' | grep -v 'toEthereumAddressOrENSNameTest.cpp' | tr '\n' ' ')
clangd-tidy -p "$COMPILE_DB" $TIDY_FILES

echo "Running clang-format --dry-run on $FILES"
../../run-clang-format.py $FILES

# CONSOLIDATED (MODERNIZATION.md Phase 2.6): the include/ tree is gone —
# the code lives in the module interface units, which are linted with
# clangd-tidy as the source of truth.
MODULE_FILES=$(find ./modules -type f -name "*.cppm" 2>/dev/null | xargs echo)
if [ -n "$MODULE_FILES" ]; then
    echo "Running clangd-tidy on $MODULE_FILES"
    clangd-tidy -p "$COMPILE_DB" $MODULE_FILES

    echo "Running clang-format --dry-run on $MODULE_FILES"
    ../../run-clang-format.py $MODULE_FILES
fi
