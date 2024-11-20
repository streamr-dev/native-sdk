#!/bin/bash

set -e

FILES=$(find . -type d \( -name src -o -name test -o -name include \) ! -path '*/build/*' ! -path '*/proto/*' -print0 | xargs -0 -I{} find {} -type f \( -name "*.hpp" -o -name "*.cpp" \) -print0 | xargs -0 echo)
echo "Running clangd-tidy on $FILES"

../../clangd-tidy/clangd-tidy -p ./build $FILES

echo "Running clang-format --dry-run on $FILES"
../../run-clang-format.py $FILES
