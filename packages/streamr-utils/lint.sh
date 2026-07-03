#!/bin/bash

set -e

FILES=$(find . -type d \( -name src -o -name include -o -name test \) ! -path '*/build/*' ! -path '*/proto/*' -print0 | xargs -0 -I{} find {} -type f \( -name "*.hpp" -o -name "*.cpp" \) -print0 | xargs -0 echo)
echo "Running clangd-tidy on $FILES"

# toEthereumAddressOrENSNameTest.cpp: clangd's experimental modules
# support cannot unify std types between its preamble and the module BMIs
# in this file (std::string crosses the module boundary in both
# directions) — excluded from clangd-tidy until clangd modules support
# matures. The compiler still fully typechecks it on every build and
# clang-format still covers it.
TIDY_FILES=$(echo "$FILES" | tr ' ' '\n' | grep -v 'toEthereumAddressOrENSNameTest.cpp' | tr '\n' ' ')
clangd-tidy -p ./build $TIDY_FILES

echo "Running clang-format --dry-run on $FILES"
../../run-clang-format.py $FILES

# Module interface units: format check only. clangd-tidy is not run on
# .cppm files (headers remain the fully linted source of truth during the
# façade migration; clangd modules support is still experimental).
MODULE_FILES=$(find ./modules -type f -name "*.cppm" 2>/dev/null | xargs echo)
if [ -n "$MODULE_FILES" ]; then
    echo "Running clang-format --dry-run on $MODULE_FILES"
    ../../run-clang-format.py $MODULE_FILES
fi
