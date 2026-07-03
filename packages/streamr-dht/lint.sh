#!/bin/bash

set -e

FILES=$(find . -type d \( -name src -o -name test -o -name include \) ! -path '*/build/*' ! -path '*/proto/*' -print0 | xargs -0 -I{} find {} -type f \( -name "*.hpp" -o -name "*.cpp" \) -print0 | xargs -0 echo)
echo "Running clangd-tidy on $FILES"

clangd-tidy -p ./build $FILES

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
